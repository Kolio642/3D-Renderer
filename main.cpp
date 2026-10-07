#include "RenderApp.h"

int main(int argc, char* argv[]) {
    
    RenderApp app;

    if(app.Initialize()) {
        app.Run();
    }

    return 0;
}