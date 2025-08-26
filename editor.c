#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"
#define MAX(a,b) ((a)>(b)?(a):(b))
#define MIN(a,b) ((a)<(b)?(a):(b))

// ---------- small utilities ----------

static void *xmalloc(uint n) {
  void *p = malloc(n);
  if(!p){ printf(2, "nen: out of memory\n"); exit(); }
  return p;
}

static char *xstrdup(const char *s) {
  int n = 0; while(s[n]) n++;
  char *p = (char*)xmalloc(n+1);
  for(int i=0;i<=n;i++) p[i]=s[i];
  return p;
}

static int strncmp_x(const char *a, const char *b, int n){
  for(int i=0;i<n;i++){
    char ca=a[i], cb=b[i];
    if(ca!=cb) return (unsigned char)ca - (unsigned char)cb;
    if(ca==0) return 0;
  }
  return 0;
}

static int strlen_x(const char *s){ int n=0; while(s[n]) n++; return n; }

static int isspace_x(char c){ return c==' '||c=='\t'||c=='\r'||c=='\n'||c=='\v'||c=='\f'; }

static int atoi_x(const char *s){
  int i=0, neg=0, v=0;
  while(isspace_x(s[i])) i++;
  if(s[i]=='-'){ neg=1; i++; }
  for(; s[i]>='0' && s[i]<='9'; i++) v = v*10 + (s[i]-'0');
  return neg? -v : v;
}

// ---------- dynamic array of lines ----------

typedef struct {
  char **data;
  int len;
  int cap;
} Lines;

static void lines_init(Lines *L){
  L->data = 0; L->len = 0; L->cap = 0;
}
static void lines_free(Lines *L){
  for(int i=0;i<L->len;i++) if(L->data[i]) free(L->data[i]);
  if(L->data) free(L->data);
  L->data=0; L->len=0; L->cap=0;
}
static void lines_reserve(Lines *L, int need){
  if(L->cap >= need) return;
  int ncap = L->cap? L->cap*2:8;
  while(ncap < need) ncap*=2;
  char **nd = (char**)xmalloc(sizeof(char*)*ncap);
  for(int i=0;i<L->len;i++) nd[i]=L->data[i];
  if(L->data) free(L->data);
  L->data = nd; L->cap = ncap;
}
static void lines_insert(Lines *L, int idx, char *line){
  if(idx<0) idx=0;
  if(idx> L->len) idx = L->len;
  lines_reserve(L, L->len+1);
  for(int i=L->len;i>idx;i--) L->data[i]=L->data[i-1];
  L->data[idx]=line;
  L->len++;
}
static void lines_push(Lines *L, char *line){ lines_insert(L, L->len, line); }
static void lines_delete_range(Lines *L, int a, int b){
  if(L->len==0) return;
  if(a<0) a=0;
  if(b>=L->len) b=L->len-1;
  if(a>b) return;
  for(int i=a;i<=b;i++) if(L->data[i]) { free(L->data[i]); L->data[i]=0; }
  int nrm = b-a+1;
  for(int i=b+1;i<L->len;i++) L->data[i-nrm]=L->data[i];
  L->len -= nrm;
}
static void lines_replace(Lines *L, int idx, char *line){
  if(idx<0 || idx>=L->len) return;
  if(L->data[idx]) free(L->data[idx]);
  L->data[idx]=line;
}

// ---------- file I/O ----------

static int read_all(int fd, char **out, int *outlen){
  int cap=1024, len=0;
  char *buf=(char*)xmalloc(cap);
  for(;;){
    if(len==cap){ cap*=2; char *nb=(char*)xmalloc(cap); for(int i=0;i<len;i++) nb[i]=buf[i]; free(buf); buf=nb; }
    int n = read(fd, buf+len, cap-len);
    if(n<0){ free(buf); return -1; }
    if(n==0) break;
    len += n;
  }
  *out=buf; *outlen=len;
  return 0;
}

static int load_file(const char *name, Lines *L){
  int fd = open(name, O_RDONLY);
  if(fd<0) return -1;
  char *buf; int n;
  if(read_all(fd, &buf, &n)<0){ close(fd); return -1; }
  close(fd);
  // split by '\n'
  int start=0;
  for(int i=0;i<n;i++){
    if(buf[i]=='\n'){
      int m = i-start;
      char *line=(char*)xmalloc(m+1);
      for(int j=0;j<m;j++) line[j]=buf[start+j];
      line[m]=0;
      lines_push(L, line);
      start=i+1;
    }
  }
  // tail if no trailing newline
  if(start<n){
    int m=n-start;
    char *line=(char*)xmalloc(m+1);
    for(int j=0;j<m;j++) line[j]=buf[start+j];
    line[m]=0;
    lines_push(L, line);
  }
  free(buf);
  return 0;
}

static int save_file(const char *name, Lines *L){
  int fd = open(name, O_WRONLY|O_CREATE|O_APPEND);
  if(fd<0) return -1;
  for(int i=0;i<L->len;i++){
    int m = strlen_x(L->data[i]);
    if(m>0) write(fd, L->data[i], m);
    if(i!=L->len-1) write(fd, "\n", 1);
  }
  close(fd);
  return 0;
}

// ---------- input helpers ----------

static char *readline(const char *prompt){
  if(prompt) printf(1, "%s", prompt);
  // read until '\n' (or EOF), grow buffer as needed
  int cap=64, len=0;
  char *s=(char*)xmalloc(cap);
  for(;;){
    char c;
    int n = read(0, &c, 1);
    if(n<1){ break; }
    if(c=='\r') c='\n';
    if(c=='\n') {
      s[len]=0;
      return s;
    }
    if(len==cap-1){
      cap*=2; char *ns=(char*)xmalloc(cap);
      for(int i=0;i<len;i++) ns[i]=s[i];
      free(s); s=ns;
    }
    s[len++]=c;
  }
  s[len]=0;
  return s;
}

static void press_enter(void){
  printf(1, "\n(press Enter) "); char *tmp=readline(""); free(tmp);
}

// ---------- UI rendering ----------

static void clear_screen(void){
  // ANSI clear screen + home. Works fine under qemu’s console.
  printf(1, "\x1b[2J\x1b[H");
}

static void draw_screen(const char *fname, Lines *L, int cur, int dirty, int top){
  clear_screen();
  int rows=24; // safe-ish for xv6 qemu default
  int header_rows=2, footer_rows=2;
  int body = rows - header_rows - footer_rows;
  if(top<0) top=0;
  if(cur<0) cur=0;
  if(cur>=L->len) cur = L->len? L->len-1 : 0;
  if(cur < top) top = cur;
  if(cur >= top + body) top = cur - body + 1;

  // header
  printf(1, "nen — %s%s  |  lines:%d  cur:%d\n", fname? fname : "(no name)", dirty? " *":"", L->len, cur+1);
  for(int i=0;i<80;i++) write(1, "-", 1);
  write(1, "\n", 1);

  // body
  for(int i=0;i<body;i++){
    int idx = top + i;
    if(idx < L->len){
      // show marker for current line
      printf(1, "%c%d | ", (idx==cur? '>':' '), idx+1);
      // print (truncate softly at ~72 cols)
      char *s = L->data[idx];
      int m = strlen_x(s);
      int limit = 72;
      for(int k=0;k<m && k<limit;k++) write(1, &s[k], 1);
      if(m>limit) printf(1, " ...");
      write(1, "\n", 1);
    } else {
      write(1, "~\n", 2);
    }
  }

  // footer / help hint
  for(int i=0;i<80;i++) write(1, "-", 1);
  write(1, "\n", 1);
  printf(1, "[j/k] move  [i] insert  [a] append  [e] edit  [d] del  [D N M] delrng  [g N] goto  [s] save  [w NAME] write-as  [r] reload  [q] quit  [h] help\n");
}

// ---------- commands ----------

static void cmd_help(void){
  printf(1,
    "Commands (enter one per line):\n"
    "  j            : move down\n"
    "  k            : move up\n"
    "  g N          : goto line N\n"
    "  i            : insert BEFORE current (end with a single '.' line)\n"
    "  a            : append AFTER current  (end with a single '.' line)\n"
    "  e            : edit/replace current line (one line)\n"
    "  d            : delete current line\n"
    "  D N M        : delete range [N..M]\n"
    "  s            : save to original filename (if any)\n"
    "  w NAME       : write buffer to NAME\n"
    "  r            : reload from disk (discard changes)\n"
    "  q            : quit (asks if unsaved changes)\n"
  );
}

static void multi_insert(Lines *L, int at){
  printf(1, "Enter lines. End with a single '.' on its own line.\n");
  for(;;){
    char *ln = readline("> ");
    if(strlen_x(ln)==1 && ln[0]=='.'){ free(ln); break; }
    lines_insert(L, at, ln);
    at++;
  }
}

static void edit_current(Lines *L, int cur){
  if(L->len==0){ lines_push(L, xstrdup("")); cur=0; }
  printf(1, "Old: %s\n", (cur>=0 && cur<L->len)? L->data[cur]:"");
  char *nl = readline("New: ");
  lines_replace(L, cur, nl);
}

// ---------- main loop ----------

int
main(int argc, char *argv[])
{
  Lines buf; lines_init(&buf);
  char *filename = 0;
  int dirty = 0;
  int cur = 0;
  int top = 0;

  if(argc >= 2){
    filename = xstrdup(argv[1]);
    if(load_file(filename, &buf)<0){
      // Start empty if file doesn’t exist yet
      // (create on save)
    }
  }

  if(buf.len==0) lines_push(&buf, xstrdup(""));

  for(;;){
    draw_screen(filename, &buf, cur, dirty, top);
    char *cmd = readline(": ");
    // trim leading spaces
    int p=0; while(isspace_x(cmd[p])) p++;
    if(cmd[p]==0){ free(cmd); continue; }

    if(cmd[p]=='h' && (cmd[p+1]==0 || isspace_x(cmd[p+1]))){
      cmd_help(); press_enter();
    }
    else if(cmd[p]=='j' && (cmd[p+1]==0 || isspace_x(cmd[p+1]))){
      if(cur < buf.len-1) cur++;
    }
    else if(cmd[p]=='k' && (cmd[p+1]==0 || isspace_x(cmd[p+1]))){
      if(cur > 0) cur--;
    }
    else if(cmd[p]=='g'){
      while(cmd[p] && !isspace_x(cmd[p])) p++;
      while(isspace_x(cmd[p])) p++;
      int n = atoi_x(cmd+p);
      if(n>=1 && n<=buf.len) cur = n-1;
    }
    else if(cmd[p]=='i' && (cmd[p+1]==0 || isspace_x(cmd[p+1]))){
      multi_insert(&buf, cur);
      dirty = 1;
    }
    else if(cmd[p]=='a' && (cmd[p+1]==0 || isspace_x(cmd[p+1]))){
      multi_insert(&buf, cur+1);
      dirty = 1;
    }
    else if(cmd[p]=='e' && (cmd[p+1]==0 || isspace_x(cmd[p+1]))){
      edit_current(&buf, cur);
      dirty = 1;
    }
    else if(cmd[p]=='d' && (cmd[p+1]==0 || isspace_x(cmd[p+1]))){
      if(buf.len>0){
        lines_delete_range(&buf, cur, cur);
        if(cur>=buf.len) cur=buf.len-1;
        if(buf.len==0) { lines_push(&buf, xstrdup("")); cur=0; }
        dirty=1;
      }
    }
    else if(cmd[p]=='D'){
      // D N M
      while(cmd[p] && !isspace_x(cmd[p])) p++;
      while(isspace_x(cmd[p])) p++;
      int n1 = atoi_x(cmd+p);
      while(cmd[p] && !isspace_x(cmd[p])) p++;
      while(isspace_x(cmd[p])) p++;
      int n2 = atoi_x(cmd+p);
      if(n1>=1 && n2>=1 && n1<=n2){
        lines_delete_range(&buf, n1-1, n2-1);
        if(cur>=buf.len) cur=buf.len-1;
        if(buf.len==0) { lines_push(&buf, xstrdup("")); cur=0; }
        dirty=1;
      }
    }
    else if(cmd[p]=='s' && (cmd[p+1]==0 || isspace_x(cmd[p+1]))){
      if(!filename){
        printf(1, "No original filename; use 'w NAME'\n");
        press_enter();
      } else {
        if(save_file(filename, &buf)<0){
          printf(1, "save failed\n"); press_enter();
        } else {
          dirty=0;
        }
      }
    }
    else if(cmd[p]=='w'){
      while(cmd[p] && !isspace_x(cmd[p])) p++;
      while(isspace_x(cmd[p])) p++;
      if(cmd[p]==0){
        printf(1, "usage: w NAME\n"); press_enter();
      } else {
        // take rest as filename
        int start=p;
        while(cmd[p] && !isspace_x(cmd[p])) p++;
        int n = p-start;
        char *nm=(char*)xmalloc(n+1);
        for(int i=0;i<n;i++) nm[i]=cmd[start+i];
        nm[n]=0;
        if(save_file(nm, &buf)<0){
          printf(1, "write failed\n"); press_enter();
        } else {
          if(!filename) filename = nm; else free(nm);
          dirty=0;
        }
      }
    }
    else if(cmd[p]=='r' && (cmd[p+1]==0 || isspace_x(cmd[p+1]))){
      if(!filename){
        printf(1, "no filename to reload\n");
        press_enter();
      } else {
        if(dirty){
          char *ans = readline("Discard changes and reload? (y/N) ");
          int yes = (ans[0]=='y'||ans[0]=='Y');
          free(ans);
          if(!yes){ free(cmd); continue; }
        }
        lines_free(&buf);
        lines_init(&buf);
        if(load_file(filename, &buf)<0){
          lines_push(&buf, xstrdup(""));
        }
        cur=0; top=0; dirty=0;
      }
    }
    else if(cmd[p]=='q' && (cmd[p+1]==0 || isspace_x(cmd[p+1]))){
      if(dirty){
        char *ans = readline("Unsaved changes. Quit anyway? (y/N) ");
        int yes = (ans[0]=='y'||ans[0]=='Y');
        free(ans);
        if(!yes){ free(cmd); continue; }
      }
      free(cmd);
      break;
    }
    else {
      printf(1, "unknown command. 'h' for help.\n");
      press_enter();
    }
    free(cmd);
  }

  if(filename) free(filename);
  lines_free(&buf);
  exit();
  return 0;
}