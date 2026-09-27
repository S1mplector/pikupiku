#define _XOPEN_SOURCE 700
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include "pikupiku.h"

typedef struct { const char *key, *label; float value, min, max; int integer; } Setting;
static Setting settings[]={
 {"strength","Overall effect",1,0,2,0}, {"amplitude","Contour displacement (pixels)",.8,0,20,0},
 {"fps","Drawings per second",6,1,60,1}, {"frames","Distinct drawings",3,2,120,1},
 {"ink","Outline darkness",.58,0,1,0}, {"grain","Stationary paper grain",1.5,0,10,0},
 {"simplify","Color simplification",.32,0,1,0}, {"width","Output width",640,32,2048,1},
 {"levels","Color levels",5,2,16,1}, {"threshold","Edge threshold",48,1,255,0},
 {"noise-scale","Contour variation spacing",12,2,100,0}, {"pressure","Pencil pressure variation",.2,0,1,0},
 {"seed","Random seed",0,0,99999,1}, {"saturation","Saturation (0 = monochrome)",1,0,2,0},
 {"contrast","Contrast",1,.1,3,0}, {"brightness","Brightness",0,-100,100,0},
 {"smoothing","Smoothing radius",2,0,5,1}, {"palette","GIF palette colors",128,4,256,1},
 {"dither","Dither (0 off, 1 Bayer)",0,0,1,1}, {"pingpong","Ping-pong loop (0 off, 1 on)",0,0,1,1}
};
#define COUNT (sizeof settings/sizeof settings[0])
static void clear_screen(void) { fputs("\033[2J\033[H",stdout); }
static void safe_print(const char *text) { for(;*text;text++) putchar(iscntrl((unsigned char)*text)?'?':*text); }
static int read_line(const char *prompt,char *buf,size_t size) {
    fputs(prompt,stdout); fflush(stdout);
    if(!fgets(buf,(int)size,stdin)) return 0;
    char *nl=strchr(buf,'\n');
    if(nl) *nl=0;
    else { int c; while((c=getchar())!='\n' && c!=EOF) {} buf[0]=0; }
    return 1;
}
static void pause_screen(void) { char line[16]; read_line("\nPress Enter to return...",line,sizeof line); }
static int valid(Setting *s,const char *text,float *value) {
    char *end; errno=0; *value=strtof(text,&end);
    return !errno && end!=text && !*end && isfinite(*value) && *value>=s->min && *value<=s->max && (!s->integer || floorf(*value)==*value);
}
static void preset(int which) {
    const float defaults[]={1,.8,6,3,.58,1.5,.32,640,5,48,12,.2,0,1,1,0,2,128,0,0};
    for(size_t i=0;i<COUNT;i++) settings[i].value=defaults[i];
    if(which==1) { settings[0].value=.4; settings[2].value=5; settings[4].value=.4; settings[5].value=0; }
    if(which==2) { settings[0].value=2; settings[3].value=5; }
    if(which==3) { settings[13].value=0; settings[14].value=1.15; settings[4].value=.7; settings[5].value=2; }
    if(which==4) { settings[0].value=0; settings[5].value=0; settings[6].value=0; settings[16].value=0; }
}
static void edit_settings(void) {
    int page=0; char line[128];
    for(;;) {
        clear_screen(); printf("PIKUPIKU / Settings %d of 2\n\n",page+1);
        for(size_t i=page*10;i<(size_t)(page+1)*10 && i<COUNT;i++)
            printf("%2zu  %-35s %7g  [%g .. %g]\n",i+1,settings[i].label,settings[i].value,settings[i].min,settings[i].max);
        if(!read_line("\nNumber to edit | n next page | b back: ",line,sizeof line) || !strcmp(line,"b")) return;
        if(!strcmp(line,"n")) { page=1-page; continue; }
        char *end; long index=strtol(line,&end,10)-1;
        if(end==line || *end || index<0 || index>=(long)COUNT) continue;
        Setting *s=&settings[index]; printf("%s (%g .. %g)\n",s->label,s->min,s->max);
        if(!read_line("New value (Enter cancels): ",line,sizeof line)) return;
        if(!*line) continue;
        float value;
        if(valid(s,line,&value)) s->value=value;
        else { puts("Invalid value; setting unchanged."); pause_screen(); }
    }
}
static void browse(char *input,size_t capacity) {
    char cwd[PATH_MAX],line[128]; if(!getcwd(cwd,sizeof cwd)) return;
    for(;;) {
        struct dirent **entries=NULL; int count=scandir(cwd,&entries,NULL,alphasort);
        if(count<0) { perror("Browse"); pause_screen(); return; }
        int page=0,done=0;
        while(!done) {
            clear_screen(); fputs("PIKUPIKU / Browse\n",stdout); safe_print(cwd); puts("\n");
            int start=page*12,limit=start+12<count?start+12:count;
            for(int i=start;i<limit;i++) { printf("%3d  ",i+1); safe_print(entries[i]->d_name); putchar('\n'); }
            if(!read_line("\nNumber opens | n next | p previous | b back: ",line,sizeof line) || !strcmp(line,"b")) { done=2; break; }
            if(!strcmp(line,"n")) { if(limit<count) page++; continue; }
            if(!strcmp(line,"p")) { if(page) page--; continue; }
            char *end; long index=strtol(line,&end,10)-1;
            if(end==line || *end || index<0 || index>=count) continue;
            char path[PATH_MAX], resolved[PATH_MAX]; struct stat st;
            if(snprintf(path,sizeof path,"%s/%s",cwd,entries[index]->d_name)>=(int)sizeof path || !realpath(path,resolved) || stat(resolved,&st)) continue;
            if(S_ISDIR(st.st_mode)) { snprintf(cwd,sizeof cwd,"%s",resolved); done=1; }
            else if(S_ISREG(st.st_mode)) { snprintf(input,capacity,"%s",resolved); done=2; }
        }
        for(int i=0;i<count;i++) free(entries[i]);
        free(entries);
        if(done==2) return;
    }
}
static void config(int save) {
    char path[PATH_MAX],line[256];
    if(!read_line(save?"Save settings to file: ":"Load settings file: ",path,sizeof path) || !*path) return;
    if(save && !access(path,F_OK)) { if(!read_line("File exists. Replace? [y/N]: ",line,sizeof line) || strcmp(line,"y")) return; }
    FILE *f=fopen(path,save?"w":"r"); if(!f) { perror("Settings"); pause_screen(); return; }
    if(save) {
        int error=0;
        for(size_t i=0;i<COUNT;i++) if(fprintf(f,"%s=%g\n",settings[i].key,settings[i].value)<0) error=1;
        if(fclose(f)) error=1;
        puts(error?"Could not save settings.":"Settings saved.");
    } else {
        Setting draft[COUNT]; memcpy(draft,settings,sizeof settings); int error=0;
        while(fgets(line,sizeof line,f)) {
            line[strcspn(line,"\r\n")]=0; if(!*line || *line=='#') continue;
            char *eq=strchr(line,'='); if(!eq) { error=1; break; } *eq++=0;
            size_t i; for(i=0;i<COUNT;i++) if(!strcmp(line,draft[i].key)) break;
            float value; if(i==COUNT || !valid(&draft[i],eq,&value)) { error=1; break; }
            draft[i].value=value;
        }
        error |= ferror(f); fclose(f);
        if(!error) memcpy(settings,draft,sizeof settings);
        puts(error?"Invalid settings file; nothing changed.":"Settings loaded.");
    }
    pause_screen();
}
static int render(const char *input,const char *output) {
    char answer[16]; int overwrite=0;
    if(!*input || !*output) { puts("Choose both input and output first."); return 0; }
    if(!access(output,F_OK)) {
        if(!read_line("Output exists. Replace? [y/N]: ",answer,sizeof answer) || strcmp(answer,"y")) return 0;
        overwrite=1;
    }
    puts("Rendering... (FFmpeg must be installed)"); fflush(stdout);
    pid_t pid=fork(); if(pid<0) { perror("Render"); return 0; }
    if(!pid) {
        char *args[2*COUNT+5], keys[COUNT][40], values[COUNT][40]; int n=0;
        args[n++]="pikupiku"; args[n++]=(char*)input; args[n++]=(char*)output;
        for(size_t i=0;i<COUNT;i++) {
            snprintf(keys[i],sizeof keys[i],"--%s",settings[i].key);
            snprintf(values[i],sizeof values[i],"%g",settings[i].value);
            args[n++]=keys[i]; args[n++]=values[i];
        }
        if(overwrite) args[n++]="--overwrite";
        args[n]=NULL;
        exit(render_cli(n,args));
    }
    int status; while(waitpid(pid,&status,0)<0) { if(errno!=EINTR) return 0; }
    return WIFEXITED(status) && WEXITSTATUS(status)==0;
}
int tui_main(void) {
    if(!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)) { fputs("TUI requires a terminal. Use --help for batch rendering.\n",stderr); return 1; }
    char input[PATH_MAX]="",output[PATH_MAX]="animation.gif",line[PATH_MAX]; int rendered=0;
    for(;;) {
        clear_screen(); puts("PIKUPIKU\nAnimated pencil contours from a still image\n");
        fputs("Input:  ",stdout); safe_print(*input?input:"Choose an image");
        fputs("\nOutput: ",stdout); safe_print(output);
        printf("\nStrength %.2g | %g drawings | %g fps\n\n",settings[0].value,settings[3].value,settings[2].value);
        puts("1  Browse for image       2  Enter image path\n3  Set output GIF path    4  Adjust settings\n5  Choose preset          6  Render GIF\n7  Open last result       8  Save settings\n9  Load settings          q  Quit");
        if(!read_line("\nChoose: ",line,sizeof line) || !strcmp(line,"q")) break;
        if(!strcmp(line,"1")) browse(input,sizeof input);
        else if(!strcmp(line,"2")) { if(read_line("Image path: ",line,sizeof line) && *line) snprintf(input,sizeof input,"%s",line); }
        else if(!strcmp(line,"3")) { if(read_line("Output GIF path: ",line,sizeof line) && *line) { snprintf(output,sizeof output,"%s",line); rendered=0; } }
        else if(!strcmp(line,"4")) edit_settings();
        else if(!strcmp(line,"5")) {
            if(read_line("0 Normal | 1 Subtle | 2 Strong | 3 Graphite | 4 Off: ",line,sizeof line) && strlen(line)==1 && *line>='0' && *line<='4') preset(*line-'0');
        } else if(!strcmp(line,"6")) { rendered=render(input,output); puts(rendered?"Render complete.":"Render cancelled or failed."); pause_screen(); }
        else if(!strcmp(line,"7")) {
            if(!rendered) { puts("Render a GIF first."); pause_screen(); continue; }
            char resolved[PATH_MAX]; if(!realpath(output,resolved)) continue;
            pid_t pid=fork(); if(!pid) {
#ifdef __APPLE__
                execlp("open","open",resolved,(char*)NULL);
#else
                execlp("xdg-open","xdg-open",resolved,(char*)NULL);
#endif
                _exit(127);
            }
            if(pid>0) { int status; waitpid(pid,&status,0); if(!WIFEXITED(status)||WEXITSTATUS(status)) { puts("Could not open the viewer."); pause_screen(); } }
        } else if(!strcmp(line,"8")) config(1);
        else if(!strcmp(line,"9")) config(0);
    }
    clear_screen(); return 0;
}
