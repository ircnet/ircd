void sendto_server(char *buf, ...); 
void sendto_user(char *text, ...);
void process_server_output(char *line);
void parse_server_output(char *buffer);
int  server_output(int fd, char *buffer);

void service_pong(void);
void service_notice(char **args);
void service_squery(char **args);
int  service_userhost(char *args);
void squery_help(char **args);
void squery_tkline(char **args);
void squery_quit(char **args);

void sendlog(char *text, ...);
char *ts(void);

int is_opered(void);
int is_authorized(char *pwd, char *host);

void exec_cmd(char *cmd, ...);
int  add_tkline(char *host, char *user, char *reason, int lifetime);
int  check_tklines(char *host, char *user, int lifetime);
void rehash(int what);
