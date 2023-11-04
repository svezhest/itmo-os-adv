// Shell.

#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"
#include "kernel/fs.h"
#include "kernel/stat.h"

// Parsed command representation
#define EXEC  1
#define REDIR 2
#define PIPE  3
#define LIST  4
#define BACK  5
#define ERROR 6

#define MAXARGS 10

struct cmd {
  int type;
};

struct execcmd {
  int type;
  char *argv[MAXARGS];
  char *eargv[MAXARGS];
};

struct redircmd {
  int type;
  struct cmd *cmd;
  char *file;
  char *efile;
  int mode;
  int fd;
};

struct pipecmd {
  int type;
  struct cmd *left;
  struct cmd *right;
};

struct listcmd {
  int type;
  struct cmd *left;
  struct cmd *right;
};

struct backcmd {
  int type;
  struct cmd *cmd;
};

int fork1(void);  // Fork but panics on failure.
void panic(char*);
struct cmd *parsecmd(char*);
struct cmd badcmd;
void
runcmd(struct cmd *cmd)
{
  int p[2];
  struct backcmd *bcmd;
  struct execcmd *ecmd;
  struct listcmd *lcmd;
  struct pipecmd *pcmd;
  struct redircmd *rcmd;

  if(cmd == 0)
    exit();

  switch(cmd->type){
  default:
    if (fork1() == 0)
    {
        panic("impossible command (internal error)");
        exit();
    }

  case EXEC:
    ecmd = (struct execcmd*)cmd;
    if(ecmd->argv[0] != 0) {
        if (fork1() == 0) {
            exec(ecmd->argv[0], ecmd->argv);
            fprintf(2, "exec %s failed\n", ecmd->argv[0]);
            exit();
        } else {
            wait();
        }
    }
    break;

  case REDIR:
    rcmd = (struct redircmd*)cmd;
    close(rcmd->fd);
    if(open(rcmd->file, rcmd->mode) < 0){
        fprintf(2, "open %s failed\n", rcmd->file);
    } else {
        runcmd(rcmd->cmd);
    }
    break;

  case LIST:
    lcmd = (struct listcmd*)cmd;
//     maybe reducable
    if(fork1() == 0)
        runcmd(lcmd->left);
    else
        wait();
    runcmd(lcmd->right);
    break;

  case PIPE:
    pcmd = (struct pipecmd*)cmd;
    if(pipe(p) < 0) {
      panic("fatal pipe error");
      exit();
    }
    if(fork1() == 0){
      close(1);
      dup(p[1]);
      close(p[0]);
      close(p[1]);
      runcmd(pcmd->left);
    }
    if(fork1() == 0){
      close(0);
      dup(p[0]);
      close(p[0]);
      close(p[1]);
      runcmd(pcmd->right);
    }
    close(p[0]);
    close(p[1]);
    wait();
    wait();
    break;

  case BACK:
    bcmd = (struct backcmd*)cmd;
    if(fork1() == 0) {
      runcmd(bcmd->cmd);
      exit();
    }
    break;
    
    case ERROR:
        panic("cannot parse command");
      return;
  }
}

char *stradd(char *from, char *to) {
    strcpy(to, from);

    return to + strlen(to);
}

int print_autofill_sugg(char *typed) {
    char *path =".";
    char buf[512], *p;
    int fd;
    struct dirent de;
    struct stat st;

    if((fd = open(path, 0)) < 0 ||
        strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
        return -1;
    }

    char output_buffer[1024], *out_pos = output_buffer;
    int match_counter = 0;

    strcpy(buf, path);
    p = buf+strlen(buf);
    *p++ = '/';
    while(read(fd, &de, sizeof(de)) == sizeof(de)){
        if(de.inum == 0) {
            continue;
        }
        memmove(p, de.name, DIRSIZ);
        p[DIRSIZ] = 0;
        if(stat(buf, &st) < 0){
            continue;
        }

//        future feature: autofill common part of the matches
        if (st.type == 2 && strncmp(buf + 2, typed, strlen(typed)) == 0) {
            ++match_counter;
            out_pos = stradd(buf + 2, out_pos);
            *out_pos = ' ';
            ++out_pos;
        }
    }
    *out_pos = '\0';
    switch (match_counter) {
        case 0:
            return -1;
        case 1:
            fprintf(2, "%s\n", output_buffer + strlen(typed));
//            if it extends over cmd buf size it is not my fault, really
            strcpy(typed, output_buffer);
            return 0;
        default:
            fprintf(2, "\n%s\n", output_buffer);
    }
    return 1;
}

int
getcmd(char *buf, int nbuf)
{
    fprintf(2, "$ ");
    memset(buf, 0, nbuf);
    int i = 0;
    char c;
//    slightly modified gets function
    while (i + 1 < nbuf) {
        if (read(0, &c, 1) < 1) {
            break;
        }
        if (c == '\t') {
            buf[i] = '\0';
            switch (print_autofill_sugg(buf)) {
                case -1:
                    continue;
                case 0:
                    return 0;
                default:
                    fprintf(2, "$ %s", buf);
            }
        } else {
            buf[i++] = c;
        }
        if(c == '\n' || c == '\r') {
            break;
        }
    }
    buf[i] = '\0';
    if(buf[0] == 0) {
        return -1;
    }
    return 0;
}

char whitespace[] = " \t\r\n\v";

int
main(void)
{
  static char buf[100];
  int fd;
  badcmd.type = ERROR;
  // Ensure that three file descriptors are open.
  while((fd = open("console", O_RDWR)) >= 0){
    if(fd >= 3){
      close(fd);
      break;
    }
  }

  // Read and run input commands.
  while(getcmd(buf, sizeof(buf)) >= 0){
    if(buf[0] == 'c' && buf[1] == 'd' && buf[2] == ' '){
      // Chdir must be called by the parent, not the child.
      buf[strlen(buf)-1] = 0;  // chop \n
      if(chdir(buf+3) < 0)
        fprintf(2, "cannot cd %s\n", buf+3);
      continue;
    } else if (buf[0] == 'w' &&
    		buf[1] == 'a' &&
    		buf[2] == 'i' &&
    		buf[3] == 't') {
    	for (char *pos = &buf[4]; *pos != 0; ++pos) {
    		if (!strchr(whitespace, *pos)) {
    			buf[strlen(buf)-1] = 0;
    			fprintf(2, "note: ignoring everything after 'wait' (got '%s')\n", buf+5);
    			break;
    		}
    	}
    	while(wait() != -1) {}
    	continue;
    }
    runcmd(parsecmd(buf));
  }
  exit();
}

void
panic(char *s)
{
  fprintf(2, "An error occured: %s\n", s);
}

int
fork1(void)
{
  int pid;

  pid = fork();
  if(pid == -1) {
    panic("fatal fork error");
    exit();
   
}
    return pid;
}

//PAGEBREAK!
// Constructors

struct cmd*
execcmd(void)
{
  struct execcmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = EXEC;
  return (struct cmd*)cmd;
}

struct cmd*
redircmd(struct cmd *subcmd, char *file, char *efile, int mode, int fd)
{
  struct redircmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = REDIR;
  cmd->cmd = subcmd;
  cmd->file = file;
  cmd->efile = efile;
  cmd->mode = mode;
  cmd->fd = fd;
  return (struct cmd*)cmd;
}

struct cmd*
pipecmd(struct cmd *left, struct cmd *right)
{
  struct pipecmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = PIPE;
  cmd->left = left;
  cmd->right = right;
  return (struct cmd*)cmd;
}

struct cmd*
listcmd(struct cmd *left, struct cmd *right)
{
  struct listcmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = LIST;
  cmd->left = left;
  cmd->right = right;
  return (struct cmd*)cmd;
}

struct cmd*
backcmd(struct cmd *subcmd)
{
  struct backcmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = BACK;
  cmd->cmd = subcmd;
  return (struct cmd*)cmd;
}
//PAGEBREAK!
// Parsing

char symbols[] = "<|>&;()";

int
gettoken(char **ps, char *es, char **q, char **eq)
{
  char *s;
  int ret;

  s = *ps;
  while(s < es && strchr(whitespace, *s))
    s++;
  if(q)
    *q = s;
  ret = *s;
  switch(*s){
  case 0:
    break;
  case '|':
  case '(':
  case ')':
  case ';':
  case '&':
  case '<':
    s++;
    break;
  case '>':
    s++;
    if(*s == '>'){
      ret = '+';
      s++;
    }
    break;
  default:
    ret = 'a';
    while(s < es && !strchr(whitespace, *s) && !strchr(symbols, *s))
      s++;
    break;
  }
  if(eq)
    *eq = s;

  while(s < es && strchr(whitespace, *s))
    s++;
  *ps = s;
  return ret;
}

int
peek(char **ps, char *es, char *toks)
{
  char *s;

  s = *ps;
  while(s < es && strchr(whitespace, *s))
    s++;
  *ps = s;
  return *s && strchr(toks, *s);
}

struct cmd *parseline(char**, char*);
struct cmd *parsepipe(char**, char*);
struct cmd *parseexec(char**, char*);
struct cmd *nulterminate(struct cmd*);

struct cmd*
parsecmd(char *s)
{
  char *es;
  struct cmd *cmd;

  es = s + strlen(s);
  cmd = parseline(&s, es);
  peek(&s, es, "");
  if(s != es){
    panic("syntax error");
    fprintf(2, "leftovers: %s\n", s);
    return &badcmd;
  }
  nulterminate(cmd);
  return cmd;
}

struct cmd*
parseline(char **ps, char *es)
{
  struct cmd *cmd;

  cmd = parsepipe(ps, es);
  while(peek(ps, es, "&")){
    gettoken(ps, es, 0, 0);
    cmd = backcmd(cmd);
  }
  if(peek(ps, es, ";")){
    gettoken(ps, es, 0, 0);
    cmd = listcmd(cmd, parseline(ps, es));
  }
  return cmd;
}

struct cmd*
parsepipe(char **ps, char *es)
{
  struct cmd *cmd;

  cmd = parseexec(ps, es);
  if(peek(ps, es, "|")){
    gettoken(ps, es, 0, 0);
    cmd = pipecmd(cmd, parsepipe(ps, es));
  }
  return cmd;
}

struct cmd*
parseredirs(struct cmd *cmd, char **ps, char *es)
{
  int tok;
  char *q, *eq;

  while(peek(ps, es, "<>")){
    tok = gettoken(ps, es, 0, 0);
    if(gettoken(ps, es, &q, &eq) != 'a') {
      panic("missing file for redirection");
      return &badcmd;
    }
    switch(tok){
    case '<':
      cmd = redircmd(cmd, q, eq, O_RDONLY, 0);
      break;
    case '>':
      cmd = redircmd(cmd, q, eq, O_WRONLY|O_CREATE, 1);
      break;
    case '+':  // >>
      cmd = redircmd(cmd, q, eq, O_WRONLY|O_CREATE, 1);
      break;
    }
  }
  return cmd;
}

struct cmd*
parseblock(char **ps, char *es)
{
  struct cmd *cmd;

  if(!peek(ps, es, "(")) {
    panic("parseblock error");
    return &badcmd;
}
  gettoken(ps, es, 0, 0);
  cmd = parseline(ps, es);
  if(!peek(ps, es, ")")) {
    panic("syntax error - missing )");
  return &badcmd;
      
}
  gettoken(ps, es, 0, 0);
  cmd = parseredirs(cmd, ps, es);
  return cmd;
}

struct cmd*
parseexec(char **ps, char *es)
{
  char *q, *eq;
  int tok, argc;
  struct execcmd *cmd;
  struct cmd *ret;

  if(peek(ps, es, "("))
    return parseblock(ps, es);

  ret = execcmd();
  cmd = (struct execcmd*)ret;

  argc = 0;
  ret = parseredirs(ret, ps, es);
  while(!peek(ps, es, "|)&;")){
    if((tok=gettoken(ps, es, &q, &eq)) == 0)
      break;
    if(tok != 'a') {
      panic("syntax error");
      return &badcmd;
    }
    cmd->argv[argc] = q;
    cmd->eargv[argc] = eq;
    argc++;
    if(argc >= MAXARGS) {
      panic("too many args");
      return &badcmd;
}
    ret = parseredirs(ret, ps, es);
  }
  cmd->argv[argc] = 0;
  cmd->eargv[argc] = 0;
  return ret;
}

// NUL-terminate all the counted strings.
struct cmd*
nulterminate(struct cmd *cmd)
{
  int i;
  struct backcmd *bcmd;
  struct execcmd *ecmd;
  struct listcmd *lcmd;
  struct pipecmd *pcmd;
  struct redircmd *rcmd;

  if(cmd == 0)
    return 0;

  switch(cmd->type){
  case EXEC:
    ecmd = (struct execcmd*)cmd;
    for(i=0; ecmd->argv[i]; i++)
      *ecmd->eargv[i] = 0;
    break;

  case REDIR:
    rcmd = (struct redircmd*)cmd;
    nulterminate(rcmd->cmd);
    *rcmd->efile = 0;
    break;

  case PIPE:
    pcmd = (struct pipecmd*)cmd;
    nulterminate(pcmd->left);
    nulterminate(pcmd->right);
    break;

  case LIST:
    lcmd = (struct listcmd*)cmd;
    nulterminate(lcmd->left);
    nulterminate(lcmd->right);
    break;

  case BACK:
    bcmd = (struct backcmd*)cmd;
    nulterminate(bcmd->cmd);
    break;
  }
  return cmd;
}
