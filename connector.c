#define _GNU_SOURCE
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>

static char tempdir[PATH_MAX];
static char cfgpath[PATH_MAX];
static void cleanup(void) { if (cfgpath[0]) unlink(cfgpath); if (tempdir[0]) rmdir(tempdir); }
static void fail(const char *message) { fprintf(stderr, "%s\n", message); exit(2); }

static const char *need(const char *key) {
  const char *value = getenv(key);
  if (!value || !*value) { fprintf(stderr, "Missing required connection field: %s\n", key); exit(2); }
  return value;
}

static int safe_email(const char *s) {
  const char *at = strchr(s, '@');
  return at && at != s && strchr(at + 1, '.') && !strchr(s, '\n') && !strchr(s, '\r');
}

static int valid_server_char(unsigned char c) {
  return isalnum(c) || c == '.' || c == '-' || c == ':' || c == '[' || c == ']';
}

static char *server_value(const char *value) {
  const char *host = value;
  if (strncmp(value, "imaps://", 8) == 0) host = value + 8;
  else if (strstr(value, "://")) fail("HIMALAYA_IMAP_URL must use IMAPS, not plaintext IMAP");
  size_t n = strlen(host);
  if (!n || n > 255 || strchr(host, '@') || strchr(host, '/') || strchr(host, '?') || strchr(host, '#'))
    fail("HIMALAYA_IMAP_URL must be a host and optional port, without credentials or a path");
  for (size_t i = 0; i < n; i++) if (!valid_server_char((unsigned char)host[i])) fail("Invalid character in HIMALAYA_IMAP_URL");
  if (host[0] == '.' || host[n-1] == '.' || host[0] == '-') fail("Invalid IMAPS host");
  return strdup(host);
}

static char *toml_quote(const char *s) {
  size_t n = strlen(s);
  if (n > 8192) fail("A connection field is too long");
  char *out = calloc(n * 6 + 3, 1);
  if (!out) fail("Out of memory");
  size_t j = 0;
  out[j++] = '"';
  for (size_t i = 0; i < n; i++) {
    unsigned char c = (unsigned char)s[i];
    if (c == '"' || c == '\\') { out[j++] = '\\'; out[j++] = (char)c; }
    else if (c == '\n') { out[j++] = '\\'; out[j++] = 'n'; }
    else if (c == '\r') { out[j++] = '\\'; out[j++] = 'r'; }
    else if (c == '\t') { out[j++] = '\\'; out[j++] = 't'; }
    else if (c < 0x20 || c == 0x7f) { snprintf(out + j, 7, "\\u%04x", c); j += 6; }
    else out[j++] = (char)c;
  }
  out[j++] = '"'; out[j] = 0;
  return out;
}

static char *locate_self(const char *arg0) {
  char resolved[PATH_MAX];
  if (strchr(arg0, '/')) {
    if (!realpath(arg0, resolved)) fail("Cannot locate connector executable");
    return strdup(resolved);
  }
  const char *path = getenv("PATH");
  if (!path) fail("PATH is unavailable");
  char *copy = strdup(path), *save = NULL;
  for (char *dir = strtok_r(copy, ":", &save); dir; dir = strtok_r(NULL, ":", &save)) {
    char candidate[PATH_MAX];
    snprintf(candidate, sizeof(candidate), "%s/%s", *dir ? dir : ".", arg0);
    if (access(candidate, X_OK) == 0 && realpath(candidate, resolved)) { free(copy); return strdup(resolved); }
  }
  free(copy); fail("Cannot locate connector executable"); return NULL;
}

static int safe_mailbox(const char *s) {
  size_t n = strlen(s);
  if (!n || n > 128) return 0;
  for (size_t i=0;i<n;i++) if (!(isalnum((unsigned char)s[i]) || strchr(" _./-", s[i]))) return 0;
  return 1;
}
static int positive_number(const char *s, long max) {
  if (!*s) return 0;
  for (const char *p=s; *p; p++) if (!isdigit((unsigned char)*p)) return 0;
  char *end=NULL; long n=strtol(s,&end,10);
  return end && !*end && n>0 && n<=max;
}

int main(int argc, char **argv) {
  atexit(cleanup);
  if (argc < 2) fail("Supported commands: check, mailboxes, envelopes [mailbox] [page] [page-size], message <id> [mailbox]");
  const char *op = argv[1], *mailbox = "INBOX", *page = "1", *size = "10", *id = NULL;
  if (!strcmp(op, "check") || !strcmp(op, "mailboxes")) { if (argc != 2) fail("This command takes no extra arguments"); }
  else if (!strcmp(op, "envelopes")) {
    if (argc > 5) fail("Usage: envelopes [mailbox] [page] [page-size]");
    if (argc > 2) mailbox=argv[2]; if (argc > 3) page=argv[3]; if (argc > 4) size=argv[4];
    if (!safe_mailbox(mailbox) || !positive_number(page, 99999) || !positive_number(size, 100)) fail("Invalid mailbox, page, or page size (maximum 100)");
  } else if (!strcmp(op, "message")) {
    if (argc < 3 || argc > 4) fail("Usage: message <numeric-id> [mailbox]");
    id=argv[2]; if (!positive_number(id, LONG_MAX)) fail("Message ID must be a positive numeric identifier");
    if (argc == 4) mailbox=argv[3]; if (!safe_mailbox(mailbox)) fail("Invalid mailbox name");
  } else fail("Supported commands: check, mailboxes, envelopes [mailbox] [page] [page-size], message <id> [mailbox]");

  const char *email=need("HIMALAYA_EMAIL"), *username=need("HIMALAYA_USERNAME"), *password=need("HIMALAYA_PASSWORD");
  if (!safe_email(email)) fail("HIMALAYA_EMAIL must be an email address");
  if (strlen(username)>512 || strlen(password)>4096) fail("A connection field is too long");
  char *server=server_value(need("HIMALAYA_IMAP_URL"));
  char *qemail=toml_quote(email), *quser=toml_quote(username), *qpass=toml_quote(password), *qserver=toml_quote(server);

  snprintf(tempdir,sizeof(tempdir),"/tmp/himalaya-nod-XXXXXX");
  if (!mkdtemp(tempdir)) fail("Cannot create a private temporary directory");
  if (chmod(tempdir, 0700)) fail("Cannot secure the temporary directory");
  snprintf(cfgpath,sizeof(cfgpath),"%s/config.toml",tempdir);
  int fd=open(cfgpath,O_WRONLY|O_CREAT|O_EXCL,0600);
  if(fd<0) fail("Cannot create a private temporary config");
  FILE *f=fdopen(fd,"w");
  if(!f) fail("Cannot write temporary config");
  fprintf(f,"[accounts.mail]\ndefault = true\nemail = %s\n\n[accounts.mail.imap]\nserver = %s\n\n[accounts.mail.imap.sasl.login]\nusername = %s\npassword.raw = %s\n",qemail,qserver,quser,qpass);
  if(fclose(f)) fail("Cannot finish temporary config");

  char *self=locate_self(argv[0]); char himalaya[PATH_MAX];
  snprintf(himalaya,sizeof(himalaya),"%s/himalaya",dirname(self));
  char *args[32]; int n=0;
  args[n++]=himalaya; args[n++]="--config"; args[n++]=cfgpath; args[n++]="--account"; args[n++]="mail"; args[n++]="--backend"; args[n++]="imap"; args[n++]="--json"; args[n++]="--log-level"; args[n++]="error";
  if(!strcmp(op,"check")) { args[n++]="envelope"; args[n++]="list"; args[n++]="--mailbox"; args[n++]="INBOX"; args[n++]="--page"; args[n++]="1"; args[n++]="--page-size"; args[n++]="1"; }
  else if(!strcmp(op,"mailboxes")) { args[n++]="mailbox"; args[n++]="list"; }
  else if(!strcmp(op,"envelopes")) { args[n++]="envelope"; args[n++]="list"; args[n++]="--mailbox"; args[n++]=(char*)mailbox; args[n++]="--page"; args[n++]=(char*)page; args[n++]="--page-size"; args[n++]=(char*)size; }
  else { args[n++]="message"; args[n++]="read"; args[n++]=(char*)id; args[n++]="--mailbox"; args[n++]=(char*)mailbox; }
  args[n]=NULL;

  pid_t child=fork(); if(child<0) fail("Cannot start Himalaya");
  if(child==0) { char homeenv[PATH_MAX+8], xdg[PATH_MAX+20]; snprintf(homeenv,sizeof(homeenv),"HOME=%s",tempdir); snprintf(xdg,sizeof(xdg),"XDG_CONFIG_HOME=%s",tempdir); char *childenv[]={homeenv,xdg,"PATH=/usr/bin:/bin",NULL}; execve(himalaya,args,childenv); perror("Cannot run Himalaya"); _exit(127); }
  int status=0; while(waitpid(child,&status,0)<0) { if(errno!=EINTR) fail("Cannot wait for Himalaya"); }
  cleanup();
  if(WIFEXITED(status)) return WEXITSTATUS(status);
  if(WIFSIGNALED(status)) return 128+WTERMSIG(status);
  return 1;
}
