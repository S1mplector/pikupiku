#include <string.h>
#include "pikupiku.h"
int main(int argc,char **argv) {
    if(argc==1 || (argc==2 && !strcmp(argv[1],"--tui"))) return tui_main();
    return render_cli(argc,argv);
}
