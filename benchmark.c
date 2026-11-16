#include <fcntl.h>
#include <math.h>
#include <sched.h>
#include <semaphore.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

/*
  This program is an adaptation of the Mandelbrot program
  from the Programming Rosetta Stone, see
  http://rosettacode.org/wiki/Mandelbrot_set
  Compile the program with:
  gcc -o mandelbrot -O4 mandelbrot.c
  Usage:

  ./mandelbrot <xmin> <xmax> <ymin> <ymax> <maxiter> <xres> <out.ppm>
  Example:
  ./mandelbrot 0.27085 0.27100 0.004640 0.004810 1000 1024 pic.ppm
  The interior of Mandelbrot set is black, the levels are gray.
  If you have very many levels, the picture is likely going to be quite
  dark. You can postprocess it to fix the palette. For instance,
  with ImageMagick you can do (assuming the picture was saved to pic.ppm):
  convert -normalize pic.ppm pic.png
  The resulting pic.png is still gray, but the levels will be nicer. You
  can also add colors, for instance:
  convert -negate -normalize -fill blue -tint 100 pic.ppm pic.png
  See http://www.imagemagick.org/Usage/color_mods/ for what ImageMagick
  can do. It can do a lot.
*/

#ifndef SCHED_EXT
#define SCHED_EXT 7
#endif

void mandelbroot() {

  struct sched_param param = {.sched_priority = 0};
  if (sched_setscheduler(0, SCHED_EXT, &param) == -1) {
    perror("Erreur : Impossible de rejoindre sched_ext");
    exit(1);
  }

  /* The window in the plane. */
  const double xmin = 0.27085;
  const double xmax = 0.27100;
  const double ymin = 0.004640;
  const double ymax = 0.004810;

  /* Maximum number of iterations, at most 65535. */
  const uint16_t maxiter = 1000;

  /* Image size, width is given, height is computed. */
  const int xres = 2048;
  const int yres = (xres * (ymax - ymin)) / (xmax - xmin);

  /* The output file name */
  const char *filename = "pic.ppm";

  /* Open the file and write the header. */
  // FILE *fp = fopen(filename, "wb");
  char *comment = "# Mandelbrot set"; /* comment should start with # */

  /*write ASCII header to the file*/
  // fprintf(fp,
  // 	"P6\n# Mandelbrot, xmin=%lf, xmax=%lf, ymin=%lf, ymax=%lf,
  // maxiter=%d\n%d\n%d\n%d\n", 	xmin, xmax, ymin, ymax, maxiter, xres,
  // yres, 	(maxiter < 256 ? 256 : maxiter));

  /* Precompute pixel width and height. */
  double dx = (xmax - xmin) / xres;
  double dy = (ymax - ymin) / yres;

  double x, y; /* Coordinates of the current point in the complex plane. */
  double u, v; /* Coordinates of the iterated point. */
  int i, j;    /* Pixel counters */
  int k;       /* Iteration counter */
  for (j = 0; j < yres; j++) {
    y = ymax - j * dy;
    for (i = 0; i < xres; i++) {
      double u = 0.0;
      double v = 0.0;
      double u2 = u * u;
      double v2 = v * v;
      x = xmin + i * dx;
      /* iterate the point */
      for (k = 1; k < maxiter && (u2 + v2 < 4.0); k++) {
        v = 2 * u * v + y;
        u = u2 - v2 + x;
        u2 = u * u;
        v2 = v * v;
      };
      /* compute  pixel color and write it to file */
      if (k >= maxiter) {
        /* interior */
        const unsigned char black[] = {0, 0, 0, 0, 0, 0};
        // fwrite(black, 6, 1, fp);
      } else {
        /* exterior */
        unsigned char color[6];
        color[0] = k >> 8;
        color[1] = k & 255;
        color[2] = k >> 8;
        color[3] = k & 255;
        color[4] = k >> 8;
        color[5] = k & 255;
        // fwrite(color, 6, 1, fp);
      };
    }
  }
  // fclose(fp);
  return;
}

#define NBPROCESS 20

int main() {
  printf("hello world\n");

  int nb = NBPROCESS;

  sem_t *start_signal = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE,
                             MAP_SHARED | MAP_ANONYMOUS, -1, 0);

  sem_init(start_signal, 1, 0);

  printf("--- Lancement du Benchmark ---\n");

  for (int i = 0; i < nb; ++i) {
    int pid = fork();
    if (pid == 0) {
      sem_wait(start_signal);
      printf("pid %d starting now\n", getpid());
      mandelbroot();
      printf("pid %d ended now\n", getpid());
      exit(0);
    }
  }

  // we wait a second to finish all forks before starting the competition
  sleep(2);

  printf("RELEASE: Starting all processes now!\n");

  for (int i = 0; i < nb; ++i) {
    sem_post(start_signal);
  }

  for (int i = 0; i < nb; ++i) {
    wait(NULL);
  }

  sem_destroy(start_signal);
  munmap(start_signal, sizeof(sem_t));

  printf("--- Fin du Benchmark ---\n");
  printf("Parent pid : %d", getpid());
  return 0;
}
