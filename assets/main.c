
#include <raylib.h>
#include <stdio.h>
#include <string.h>

#define MAX_LEN 300
int main(int argc, char **argv) {
  Image img;
  char new_name[MAX_LEN];
  char *png;

  if (argc != 2) {
    printf(" !! Must name file to be exported: %s [filename]", argv[0]);
    return 1;
  }

  strncpy(new_name, argv[1], MAX_LEN);

  png = strstr(new_name, ".png");

  if (png == NULL) {
    printf(" !! Must name a PNG file to be exported: %s [filename].png", argv[0]);
    return 1;
  }

  png[1]  = 'h';
  png[2] = '\0';

  img = LoadImage(argv[1]);

  if (ExportImageAsCode(img, new_name)) {
    printf("Exported file %s to %s\n", argv[1], new_name);
  } else {
    printf(" !! Failed to export file %s\n", argv[1]);
  }
  UnloadImage(img);
}
