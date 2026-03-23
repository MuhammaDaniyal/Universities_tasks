#include <omp.h>
#include <stdio.h>

void render_graphics() {
    printf("Rendering graphics...\n");
}
void play_audio() {
    printf("Playing audio...\n");
}
void read_input() {
    printf("Reading input...\n");
}

int main() {

    #pragma omp parallel
    {
        #pragma omp sections
        {
            #pragma omp section
            { render_graphics(); }

            #pragma omp section
            { play_audio(); }

            #pragma omp section
            { read_input(); }
        }
    }

}