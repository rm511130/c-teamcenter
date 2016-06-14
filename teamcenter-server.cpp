#include <cppcms/application.h>
#include <cppcms/service.h>
#include <cppcms/http_response.h>
#include <cppcms/url_dispatcher.h>
#include <cppcms/url_mapper.h>
#include <cppcms/applications_pool.h>
#include <iostream>
#include <stdlib.h>
#include <cstdlib>
#include <string>
#include <stdio.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>
#include <sys/resource.h>

extern char **environ;

char redis_prefix[100];
char tc_id[100];
char env_name[100][1000];
char env_value[100][1000];
char cmd_output[1000][1000];

char *sn(char *a) /* takes a pointer to absbd":"123 and returns a pointer to 123 */
{
  for(;*a!='"';a++); /* finds " */
  for(a++;*a!='"';a++); /* finds next " */
  return (++a); /* returns the fisrt char after the " */
}

int log_history(char *quoi)
{
  char strgy[1000];
  time_t t = time(NULL);
  struct tm *tm = localtime(&t);
  char *quand = asctime(tm);

  quand[strlen(quand) - 1] = 0;
  sprintf(strgy,"%s RPUSH HISTORY_LOG \" %s %s \" ",redis_prefix, quand, quoi );
  system(strgy); /* Recording History of events in HISTORY_LOG List */

  return(0);
}

int exec_cmd(char *your_cmd)
{
  FILE *fp;
  int i=0;
  
  log_history(your_cmd);

  fp = popen(your_cmd, "r");
  if (fp == NULL) {  fprintf(stderr,"ERROR: Failed to popen to execute command: %s\n",your_cmd);  fflush(stderr); exit(0); };

  while (fgets(cmd_output[i], 999, fp) != NULL) i++;
  cmd_output[i][0]='\0';
  pclose(fp);
  
  return 0;
}

int redis_cmdstr()
{
  int j=1,i=1;
  char *ss = *environ; 
  char *a,*b;
  char redis_host[1000], redis_port[8], redis_password[100]; 
  char *vcap_services, *vcap_application;

  for (j=1; ss; j++)  { 
                        for (a=ss,i=0;*a!='='&&*a!='\0';a++,i++);
                        strncpy(env_name[j], ss, i);
                        env_name[j][i]='\0';
                        strcpy(env_value[j],++a);
                        if (j==19) vcap_services = env_value[j];
                        if (j==23) vcap_application = env_value[j];
                        ss = *(environ+j);
                      };

  if (strstr(vcap_services,"p-redis")!=NULL)
          {
            a=strstr(vcap_application,"teamcenter-user"); a=a+16; for(b=a,i=1; *b!='"'; b++,i++);strncpy(tc_id,a,i); tc_id[i-1]='\0';
            a=strstr(vcap_services,"host"); a=a+7; for(b=a,i=1; *b!='"'; b++,i++); strncpy(redis_host,a,i); redis_host[i-1]='\0';
            a=strstr(vcap_services,"password"); a=a+11; for(b=a,i=1; *b!='"'; b++,i++); strncpy(redis_password,a,i); redis_password[i-1]='\0';
            a=strstr(vcap_services,"port"); a=a+6;  for(b=a,i=1; *b!='}'; b++,i++); strncpy(redis_port,a,i); redis_port[i-1]='\0';
            sprintf(redis_prefix,"/home/vcap/app/redis-cli -h %s -p %s -a %s ",redis_host,redis_port,redis_password);
          }
   else   if (strstr(vcap_services,"rediscloud")!=NULL)
          {
            a=strstr(vcap_application,"teamcenter-user"); a=a+16; for(b=a,i=1; *b!='"'&&*b!='.'; b++,i++); strncpy(tc_id,a,i); tc_id[i-1]='\0';
            a=strstr(vcap_services,"hostname"); for(b=sn(a),i=1; *b!='"'; b++,i++); strncpy(redis_host,sn(a),i); redis_host[i-1]='\0';
            a=strstr(vcap_services,"password"); for(b=sn(a),i=1; *b!='"'; b++,i++); strncpy(redis_password,sn(a),i); redis_password[i-1]='\0';
            a=strstr(vcap_services,"port");     for(b=sn(a),i=1; *b!='}'&&*b!='"'; b++,i++); strncpy(redis_port,sn(a),i); redis_port[i-1]='\0';
            sprintf(redis_prefix,"/home/vcap/app/redis-cli -h %s -p %s -a %s ",redis_host,redis_port,redis_password);
          }
            else { 
                   fprintf(stderr,"Redis is not active\n");
                   redis_prefix[0]='\0';
                 };

  return (*redis_prefix?1:0);
};

double get_process_time() {
    struct rusage usage;
    if( 0 == getrusage(RUSAGE_SELF, &usage) ) {
        return (double)(usage.ru_utime.tv_sec + usage.ru_stime.tv_sec) +
               (double)(usage.ru_utime.tv_usec + usage.ru_stime.tv_usec) / 1.0e6;
    }
    return 0;
};

class hello: public cppcms::application {
public:
    hello(cppcms::service &srv) :
        cppcms::application(srv) 
    {
        dispatcher().assign("/consume_memory/(\\d+)",&hello::consume_memory,this,1); 
        mapper().assign("consume_memory","/consume_memory/{1}");
        
        dispatcher().assign("/env",&hello::environment,this);
        mapper().assign("environment","/environment");

        dispatcher().assign("/",&hello::welcome,this);
        mapper().assign("");

        dispatcher().assign("/debug",&hello::debug,this);
        mapper().assign("debug","/debug");
        
        dispatcher().assign("/kill_container",&hello::kill_container,this);
        mapper().assign("kill_container","/kill_container");

        dispatcher().assign("/clean_exit",&hello::clean_exit,this);
        mapper().assign("clean_exit","/clean_exit");

        dispatcher().assign("/redis_flushall",&hello::redis_flushall,this);
        mapper().assign("redis_flushall","/redis_flushall");

        mapper().root("/");
    }

    void consume_memory(std::string num)
    { 
        char *ptr;
        int random_mem_filler,f;
        double t_begin, t_end;
        char strg[1000];
        int hist_count;

        redis_cmdstr();

        int mem_size=atoi(num.c_str());
        srand((unsigned int)time(NULL)); 
        random_mem_filler=rand() % 9 + 1;
        t_begin = get_process_time();               

        if ((ptr=(char *)malloc(mem_size*1024*1024)))   /* allocating memory */
               { 
                 for (f=0; f<mem_size*1024*1024; f++) *ptr++=random_mem_filler;
                 t_end=get_process_time();
                 sprintf(strg,"Elapsed time: %.6f seconds\n", t_end - t_begin);
                 response().out() << strg << "<br>\n";
                 response().out() << mem_size << "MB of RAM reserved and filled with " << random_mem_filler <<"s <br>\n";

                 sprintf(strg,"%s HINCRBY USER%s:HISTORY_COUNT COUNT 1",redis_prefix,tc_id); exec_cmd(strg);
                 sprintf(strg,"%s HGET USER%s:HISTORY_COUNT COUNT",redis_prefix,tc_id); exec_cmd(strg);
                 hist_count = atoi(cmd_output[0]);

                 sprintf(strg,"%s HMSET USER%s:%d DESC MALLOC SIZE %d FILLER %d",redis_prefix,tc_id,hist_count,mem_size,random_mem_filler);
                 /* response().out() << "DEBUGGING: the cmd is... " << strg << "<br>\n"; */
                 exec_cmd(strg);
               }
              else response().out() << "Failed to reserve memory <br>\n";

        response().out() << "<a href='" << url("/") << "'>Go back</a>";
    }

    void environment()
    {
        int j;
        char *s = *environ;

        redis_cmdstr();

        response().out() << " <!DOCTYPE html> <html> <head> <style> ";
        response().out() << " table { width:100%; } table, th, td { border: 1px solid black; border-collapse: collapse; } ";
        response().out() << " th, td { padding: 5px; text-align: left; } ";
        response().out() << " table#t01 tr:nth-child(even) { background-color: #eee; } ";
        response().out() << " table#t01 tr:nth-child(odd)  { background-color: #fff; } ";
        response().out() << " table#t01 th { background-color: black; color: white; }  ";
        response().out() << " </style> </head> <body> <table id=\"t01\">  <tr> <th>Environment Variable</th> <th>Value</th> ";

        for (j=1;s;j++) {
                         response().out() << "<tr> <td>" << env_name[j] << "</td> ";
                         response().out() <<      "<td>" << env_value[j]  << "</td> ";
                         s = *(environ+j);
                        }

        response().out() << "</body> ";
        response().out() << "<a href='" << url("/") << "'>Go back</a>";
        response().out() << "</html>";
    }
    
    void welcome()
    {
        int j;
        char *s = *environ;

        redis_cmdstr();

        response().out() << "<h1> Teamcenter Server - User " << tc_id << " - APIs:  </h1>\n";
        response().out() << "<a href= '" << url("/") << "consume_memory/500'>Consume RAM: 500MB</a> ";
        response().out() << "<a href= '" << url("/") << "consume_memory/400'>400MB</a> ";
        response().out() << "<a href= '" << url("/") << "consume_memory/300'>300MB</a> ";
        response().out() << "<a href= '" << url("/") << "consume_memory/200'>200MB</a> ";
        response().out() << "<a href= '" << url("/") << "consume_memory/100'>100MB</a></br>\n";
        response().out() << "<a href= '" << url("/") << "env'>All Environment Variables</a></br>\n";
        response().out() << "<a href= '" << url("/") << "debug'>Debug Information</a></br>\n";
        response().out() << "<a href= '" << url("/") << "clean_exit'>Clean Exit</a></br>\n";
        response().out() << "<a href= '" << url("/") << "redis_flushall'>Redis Flushall</a></br>\n";
        response().out() << "<a href= '" << url("/") << "kill_container'>Kill Container</a></br></br>\n";
        response().out() << " <!DOCTYPE html> <html> <head> <style> ";
        response().out() << " table { width:100%; } table, th, td { border: 1px solid black; border-collapse: collapse; } ";
        response().out() << " th, td { padding: 5px; text-align: left; } ";
        response().out() << " table#t01 tr:nth-child(even) { background-color: #eee; } ";
        response().out() << " table#t01 tr:nth-child(odd)  { background-color: #fff; } ";
        response().out() << " table#t01 th { background-color: black; color: white; }  ";
        response().out() << " </style> </head> <body> <table id=\"t01\">  <tr> <th>Environment Variable</th> <th>Value</th> ";

        for (j=1; s; j++) {

                     switch (j)     { case 1 : /* "CF_INSTANCE_ADDR"  */
                                      case 6 : /* "CF_INSTANCE_GUID"  */
                                      case 7 : /* "PWD"               */
                                      case 8 : /* "MEMORY_LIMIT"      */
                                      case 12: /* "USER"              */
                                      case 13: /* "CF_INSTANCE_INDEX" */
                                      case 19: /* "VCAP_SERVICES"     */
                                      case 22: /* "LANG"              */
                                                     response().out() << "<tr> <td>" << env_name[j] << "</td> ";
                                                     response().out() <<      "<td>" << env_value[j]  << "</td> ";
                                   };

                     s = *(environ+j);
                   }

        response().out() << "</body> ";
        response().out() << "<a href='" << url("/") << "'>Go back</a>";
        response().out() << "</html>";
    }

    void debug()
    {   
        char strg[1000];
        int j, hist_count;

        redis_cmdstr();

        response().out() << "DEBUGGING: redis_cmdstr() returned: " << redis_prefix << "<br>\n";
        response().out() << "DEBUGGING: redis_cmdstr() returned: tc_id = " << tc_id << "<br>\n";

        sprintf(strg,"%s HGET USER%s:HISTORY_COUNT COUNT",redis_prefix,tc_id); exec_cmd(strg);
        hist_count = atoi(cmd_output[0]); 
        response().out() << "DEBUGGING: current history depth: " << hist_count << "<br><br>\n";               

        sprintf(strg,"%s LRANGE HISTORY_LOG 0 -1 ",redis_prefix); exec_cmd(strg);
        response().out() << "System Logs of Redis commands<br>\n";
        response().out() << "<div style=\"height:400px;width:1200px;border:1px solid #ccc;overflow:auto;\">";
        for (j=0;cmd_output[j][0]!=0;j++) response().out() << cmd_output[j] << "<br>\n";
        response().out() << " </div>";
 
        response().out() << "<a href='" << url("/") << "'>Go back</a>";

    }

    void kill_container()
    {   
        int i;
        char strg[1000];
        pid_t pid;  

        sprintf(strg,"%s HINCRBY USER%s:STATE STATE 1",redis_prefix,tc_id); exec_cmd(strg);
        /* USER1:STATE STATE > 1 recovering from crashed state */

        response().out() << "WARNING: Crashing Container in 2 seconds <br>\n";
        response().out() << "<a href='" << url("/") << "'>Go back</a>";
        
        pid = fork ();

        if (pid < 0) 
             { fprintf(stderr,"Error forking a process with kill_container() function\n"); 
               fflush(stderr);
             }
        else if (pid == 0) { sleep(1); for(i=18; i<100; i++) { sprintf(strg,"kill -9 %d",i); system(strg); }; }; 

    }

    void clean_exit()
    {  
        int i;
        char strg[1000];
        pid_t pid;

        sprintf(strg,"%s HSET USER%s:STATE STATE 1",redis_prefix,tc_id); exec_cmd(strg);
        /* USER1:STATE STATE = 1 clean state */

        response().out() << "WARNING: Orderly Shutdown of Container in 2 seconds <br>\n";
        response().out() << "<a href='" << url("/") << "'>Go back</a>";

        sprintf(strg,"%s HSET USER%s:HISTORY_COUNT COUNT 0",redis_prefix,tc_id); exec_cmd(strg);
        
        pid = fork ();

        if (pid < 0)
             { fprintf(stderr,"Error forking a process with kill_container() function\n");
               fflush(stderr);
             }
        else if (pid == 0) { sleep(1); for(i=18; i<100; i++) { sprintf(strg,"kill -9 %d",i); system(strg); }; };

    }

    void redis_flushall()
    {
        int i;
        char strg[1000];
        pid_t pid;

        sprintf(strg,"%s FLUSHALL",redis_prefix); exec_cmd(strg);

        sprintf(strg,"%s HSET USER%s:STATE STATE 1",redis_prefix,tc_id); exec_cmd(strg);
        /* USER1:STATE STATE = 1 clean state */

        response().out() << "WARNING: Orderly Shutdown of Container in 2 seconds <br>\n";
        response().out() << "<a href='" << url("/") << "'>Go back</a>";

        sprintf(strg,"%s HSET USER%s:HISTORY_COUNT COUNT 0",redis_prefix,tc_id); exec_cmd(strg);

        pid = fork ();

        if (pid < 0)
             { fprintf(stderr,"Error forking a process with kill_container() function\n");
               fflush(stderr);
             }
        else if (pid == 0) { sleep(1); for(i=18; i<100; i++) { sprintf(strg,"kill -9 %d",i); system(strg); }; };

    }

};


int main(int argc,char ** argv)
{
    int state, history_count, i,f,mem_size,filler;
    char *ptr;
    char strg[1000];

    redis_cmdstr();

                 sprintf(strg,"%s HEXISTS USER%s:STATE STATE",redis_prefix,tc_id); exec_cmd(strg);
                 state = atoi(cmd_output[0]);

                 if (state==0) /* HEXISTS did not find USER1:STATE STATE */
                   {
                     sprintf(strg,"%s HSET USER%s:STATE STATE 1",redis_prefix,tc_id); exec_cmd(strg);
                     /* HSET USER1:STATE STATE to 1 indicating clean state */
                   };

                 sprintf(strg,"%s HGET USER%s:STATE STATE",redis_prefix,tc_id); exec_cmd(strg);
                 state = atoi(cmd_output[0]);

                 if (state!=1) /* ASSUME SYSTEM CRASHED AND NEEDS TO BE REBUILT */
                   {
                     sprintf(strg,"%s HGET USER%s:HISTORY_COUNT COUNT",redis_prefix,tc_id); exec_cmd(strg);
                     history_count = atoi(cmd_output[0]); 

                     for(i=1;i<=history_count;i++)
                        {
                          sprintf(strg,"%s HMGET USER%s:%d SIZE FILLER",redis_prefix,tc_id,i); exec_cmd(strg);
                          mem_size = atoi(cmd_output[0]); 
                          filler = atoi(cmd_output[1]);
                          
                          ptr=(char *)malloc(mem_size*1024*1024);   /* allocating memory */
                          for (f=0; f<mem_size*1024*1024; f++) *ptr++=filler; /* filling it up */
                        }

                     sprintf(strg,"%s HSET USER%s:STATE STATE 1",redis_prefix,tc_id); exec_cmd(strg);
                   }

    try {
        cppcms::service app(argc,argv);
        app.applications_pool().mount(cppcms::applications_factory<hello>());
        app.run();
    }
    catch(std::exception const &e) {
        std::cerr<<e.what()<<std::endl;
    }
}
