#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h>

static char temporary_dir[256];
static void cleanup(void) {
    if (!*temporary_dir) return;
    const char *names[]={"input.rgb","frames.rgb","result.gif"};
    char path[300];
    for(size_t i=0;i<3;i++) { snprintf(path,sizeof path,"%s/%s",temporary_dir,names[i]); unlink(path); }
    rmdir(temporary_dir);
}
static void fail(const char *message) { fprintf(stderr, "pikupiku: %s\n", message); exit(1); }
static void run(char *const args[]) {
    pid_t pid = fork();
    if (pid < 0) fail("cannot fork");
    if (!pid) { execvp(args[0], args); perror(args[0]); _exit(127); }
    int status;
    if (waitpid(pid, &status, 0) < 0 || !WIFEXITED(status) || WEXITSTATUS(status)) fail("ffmpeg failed");
}
static uint8_t clamp(float x) { return x < 0 ? 0 : x > 255 ? 255 : (uint8_t)(x + .5f); }
static uint32_t hash(int x, int y, int seed) {
    uint32_t n = (uint32_t)x * 0x9e3779b1u ^ (uint32_t)y * 0x85ebca77u ^ (uint32_t)seed * 0xc2b2ae3du;
    n ^= n >> 16; n *= 0x7feb352du; n ^= n >> 15; n *= 0x846ca68bu; return n ^ (n >> 16);
}
static float noise(int x, int y, int seed) { return (hash(x, y, seed) & 65535) / 32767.5f - 1.f; }
static float smooth_noise(float x, float y, int seed) {
    int ix=(int)floorf(x), iy=(int)floorf(y);
    float fx=x-ix, fy=y-iy;
    fx=fx*fx*(3-2*fx); fy=fy*fy*(3-2*fy);
    float a=noise(ix,iy,seed), b=noise(ix+1,iy,seed);
    float c=noise(ix,iy+1,seed), d=noise(ix+1,iy+1,seed);
    return (a+(b-a)*fx)*(1-fy)+(c+(d-c)*fx)*fy;
}
static float luminance(const uint8_t *p) { return .2126f*p[0] + .7152f*p[1] + .0722f*p[2]; }
static int sample(int v, int max) { return v < 0 ? 0 : v >= max ? max-1 : v; }

static float number(const char *s) {
    char *end; errno=0; float value=strtof(s,&end);
    if(errno || end==s || *end || !isfinite(value)) fail("invalid numeric option");
    return value;
}

int render_cli(int argc, char **argv) {
    const char *input = NULL, *output = NULL;
    int width = 512, frames = 3, fps = 6, levels = 5;
    float amplitude = 0.8f, strength=1.f, ink_darkness=.58f, grain_amount=1.5f, simplify=.32f;
    float threshold=48, noise_scale=12, pressure_variation=.2f, saturation=1, contrast=1, brightness=0;
    int seed=0, smoothing=2, palette=128, dither=0, pingpong=0, overwrite=0;
    for (int i=1; i<argc; ++i) {
        if(!strcmp(argv[i],"--overwrite")) { overwrite=1; continue; }
        if (!strcmp(argv[i],"--help")) {
            puts("usage: pikupiku INPUT OUTPUT.gif [--strength 0..2] [--amplitude 0..20] [--fps 1..60] [--frames 2..120] [--ink 0..1] [--grain 0..10] [--simplify 0..1] [--width 32..2048] [--levels 2..16] [--threshold 1..255] [--noise-scale 2..100] [--pressure 0..1] [--seed 0..99999] [--saturation 0..2] [--contrast 0.1..3] [--brightness -100..100] [--smoothing 0..5] [--palette 4..256] [--dither 0..1] [--pingpong 0..1] [--overwrite]");
            return 0;
        }
        if (!strncmp(argv[i],"--",2)) {
            const char *option=argv[i];
            if(++i>=argc) fail("option requires a value");
            float value=number(argv[i]);
            if (!strcmp(option,"--width") || !strcmp(option,"--frames") || !strcmp(option,"--fps") || !strcmp(option,"--levels")) {
                if(value!=floorf(value) || value<0 || value>2048) fail("integer option out of range");
                if(!strcmp(option,"--width")) width=(int)value;
                else if(!strcmp(option,"--frames")) frames=(int)value;
                else if(!strcmp(option,"--fps")) fps=(int)value;
                else levels=(int)value;
            } else if(!strcmp(option,"--amplitude")) amplitude=value;
            else if(!strcmp(option,"--strength")) strength=value;
            else if(!strcmp(option,"--ink")) ink_darkness=value;
            else if(!strcmp(option,"--grain")) grain_amount=value;
            else if(!strcmp(option,"--simplify")) simplify=value;
            else if(!strcmp(option,"--threshold")) threshold=value;
            else if(!strcmp(option,"--noise-scale")) noise_scale=value;
            else if(!strcmp(option,"--pressure")) pressure_variation=value;
            else if(!strcmp(option,"--saturation")) saturation=value;
            else if(!strcmp(option,"--contrast")) contrast=value;
            else if(!strcmp(option,"--brightness")) brightness=value;
            else if(!strcmp(option,"--seed") || !strcmp(option,"--smoothing") || !strcmp(option,"--palette") || !strcmp(option,"--dither") || !strcmp(option,"--pingpong")) {
                if(value!=floorf(value) || value<0 || value>99999) fail("integer option out of range");
                if(!strcmp(option,"--seed")) seed=(int)value;
                else if(!strcmp(option,"--smoothing")) smoothing=(int)value;
                else if(!strcmp(option,"--palette")) palette=(int)value;
                else if(!strcmp(option,"--dither")) dither=(int)value;
                else pingpong=(int)value;
            }
            else fail("unknown option (see --help)");
        } else if (!input) input=argv[i]; else if (!output) output=argv[i]; else fail("unexpected argument");
    }
    if (!input || !output || width<32 || width>2048 || frames<2 || frames>120 || fps<1 || fps>60 || levels<2 || levels>16 || amplitude<0 || amplitude>20 || strength<0 || strength>2 || ink_darkness<0 || ink_darkness>1 || grain_amount<0 || grain_amount>10 || simplify<0 || simplify>1)
        fail("missing input/output or option out of range (see --help)");
    if(threshold<1 || threshold>255 || noise_scale<2 || noise_scale>100 || pressure_variation<0 || pressure_variation>1 || saturation<0 || saturation>2 || contrast<.1f || contrast>3 || brightness< -100 || brightness>100 || smoothing>5 || palette<4 || palette>256 || dither>1 || pingpong>1) fail("option out of range");
    struct stat input_stat, output_stat;
    if(stat(input,&input_stat) || !S_ISREG(input_stat.st_mode)) fail("input must be a local regular file");
    if(!stat(output,&output_stat)) {
        if(input_stat.st_dev==output_stat.st_dev && input_stat.st_ino==output_stat.st_ino) fail("input and output must differ");
        if(!overwrite) fail("output already exists; choose another name or use --overwrite");
    }
    char dir[]="/tmp/pikupiku-XXXXXX";
    if (!mkdtemp(dir)) fail("cannot create temporary directory");
    snprintf(temporary_dir,sizeof temporary_dir,"%s",dir); atexit(cleanup);
    char raw[256], video[256], scale[80], rate[24];
    snprintf(raw,sizeof raw,"%s/input.rgb",dir); snprintf(video,sizeof video,"%s/frames.rgb",dir);
    snprintf(scale,sizeof scale,"scale=%d:-2:flags=lanczos,format=rgb24",width);
    char *decode[]={"ffmpeg","-hide_banner","-loglevel","error","-y","-i",(char*)input,"-frames:v","1","-vf",scale,"-f","rawvideo","-pix_fmt","rgb24",raw,NULL};
    run(decode);
    struct stat info;
    if(stat(raw,&info) || info.st_size<=0 || info.st_size%(width*3)) fail("cannot read decoded image size");
    int height=(int)(info.st_size/(width*3));
    if(height<2 || height>4096) fail("scaled image height out of range");
    size_t pixels=(size_t)width*height, bytes=pixels*3;
    uint8_t *src=malloc(bytes), *flat=malloc(bytes), *out=malloc(bytes), *edge=malloc(pixels);
    if(!src||!flat||!out||!edge) fail("out of memory");
    FILE *f=fopen(raw,"rb"); if(!f || fread(src,1,bytes,f)!=bytes) fail("cannot decode image"); fclose(f);
    /* Gentle local averaging and palette reduction give a hand-painted base. */
    for(int y=0;y<height;y++) for(int x=0;x<width;x++) {
        float sum[3]={0}, weight=0; const uint8_t *center=src+3*((size_t)y*width+x);
        for(int dy=-smoothing;dy<=smoothing;dy++) for(int dx=-smoothing;dx<=smoothing;dx++) {
            const uint8_t *p=src+3*((size_t)sample(y+dy,height)*width+sample(x+dx,width));
            float color=fabsf(luminance(p)-luminance(center)); float w=1.f/(1.f+color*.05f);
            for(int c=0;c<3;c++) sum[c]+=w*p[c];
            weight+=w;
        }
        for(int c=0;c<3;c++) {
            float v=sum[c]/weight, step=255.f/(levels-1);
            flat[3*((size_t)y*width+x)+c]=clamp((1-simplify)*v+simplify*roundf(v/step)*step);
        }
    }
    for(int y=0;y<height;y++) for(int x=0;x<width;x++) {
        float left=luminance(flat+3*((size_t)y*width+sample(x-1,width)));
        float right=luminance(flat+3*((size_t)y*width+sample(x+1,width)));
        float up=luminance(flat+3*((size_t)sample(y-1,height)*width+x));
        float down=luminance(flat+3*((size_t)sample(y+1,height)*width+x));
        float strength=hypotf(right-left,down-up);
        edge[(size_t)y*width+x]=(uint8_t)(strength>threshold ? 1:0);
    }
    f=fopen(video,"wb"); if(!f) fail("cannot write frames");
    int total_frames=pingpong ? frames*2-2 : frames;
    for(int frame=0;frame<total_frames;frame++) {
        int t=frame<frames ? frame : total_frames-frame;
        /* Independent held drawings: only the added pencil contour changes.
           The source colors, landmarks, and silhouette never move. */
        for(int y=0;y<height;y++) for(int x=0;x<width;x++) {
            float sx=x+strength*amplitude*smooth_noise(x/noise_scale,y/noise_scale,71+t*103+seed);
            float sy=y+strength*amplitude*smooth_noise(x/noise_scale,y/noise_scale,193+t*107+seed);
            int ix=(int)floorf(sx), iy=(int)floorf(sy);
            float fx=sx-ix, fy=sy-iy, ink=0;
            for(int dy=0;dy<=1;dy++) for(int dx=0;dx<=1;dx++) {
                float w=(dx?fx:1-fx)*(dy?fy:1-fy);
                ink+=w*edge[(size_t)sample(iy+dy,height)*width+sample(ix+dx,width)];
            }
            float pressure=fminf(1.f,strength*ink_darkness*(1.f+pressure_variation*smooth_noise(x/8.f,y/8.f,313+t*29+seed)));
            float grain=strength*grain_amount*noise(x,y,41+seed); /* paper remains still */
            size_t to=(size_t)y*width+x;
            for(int c=0;c<3;c++) {
                float mix=fminf(1.f,strength);
                float base=(1-mix)*src[3*to+c]+mix*flat[3*to+c];
                float gray=luminance(flat+3*to);
                base=gray+(base-gray)*saturation;
                base=(base-127.5f)*contrast+127.5f+brightness;
                out[3*to+c]=clamp(base*(1-ink*pressure)+28*ink*pressure+grain);
            }
        }
        if(fwrite(out,1,bytes,f)!=bytes) fail("cannot write frame");
    }
    fclose(f); snprintf(rate,sizeof rate,"%d",fps);
    char dims[40]; snprintf(dims,sizeof dims,"%dx%d",width,height);
    char filter[256], result[256];
    snprintf(result,sizeof result,"%s/result.gif",dir);
    snprintf(filter,sizeof filter,"[0:v]split[a][b];[a]palettegen=max_colors=%d[p];[b][p]paletteuse=dither=%s",palette,dither?"bayer":"none");
    char *encode[]={"ffmpeg","-hide_banner","-loglevel","error","-y","-f","rawvideo","-pix_fmt","rgb24","-s",dims,"-r",rate,"-i",video,"-filter_complex",filter,"-loop","0",result,NULL};
    run(encode);
    /* Publish only a completely encoded GIF. Never truncate an existing file on render failure. */
    char *staged=malloc(strlen(output)+16); if(!staged) fail("out of memory");
    sprintf(staged,"%s.tmp-XXXXXX",output);
    int fd=mkstemp(staged); if(fd<0) fail("cannot create output file");
    FILE *dst=fdopen(fd,"wb"), *encoded=fopen(result,"rb");
    if(!dst || !encoded) { unlink(staged); fail("cannot copy encoded GIF"); }
    char buffer[65536]; size_t count; int copy_failed=0;
    while((count=fread(buffer,1,sizeof buffer,encoded))) if(fwrite(buffer,1,count,dst)!=count) { copy_failed=1; break; }
    copy_failed |= ferror(encoded); fclose(encoded);
    if(fclose(dst)) copy_failed=1;
    if(copy_failed) { unlink(staged); fail("output write failed"); }
    if(overwrite) { if(rename(staged,output)) { unlink(staged); fail("cannot replace output"); } }
    else { if(link(staged,output)) { unlink(staged); fail("cannot publish output (already exists or filesystem error)"); } unlink(staged); }
    free(staged); cleanup(); temporary_dir[0]=0;
    free(src); free(flat); free(out); free(edge);
    printf("Created %s (%dx%d, %d frames at %d fps)\n",output,width,height,total_frames,fps);
    return 0;
}
