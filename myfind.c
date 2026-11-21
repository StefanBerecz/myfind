#include <stdio.h>
//include für getopt()
#include <unistd.h>
#include <assert.h>
#include <sys/stat.h>
//für realpath()
//für EXIT_FAILURE/EXIT_SUCCESS
#include <stdlib.h>
//für directory operations
#include <dirent.h>
//für strcmp() - strings vergleichen
#include <string.h>


int searchFile(const char *path, const char *fileName) {
  int found = 0;
  struct dirent *entry;
  DIR *dir = opendir(path);
  if (dir == NULL) {
    perror(path);
    return -1;
  }
  while ((entry = readdir(dir)) != NULL) {
    if (strcmp(entry->d_name, fileName) == 0) {
      //Dieses output wird umformuliert und in einem nameless pipe gefürht.
      printf("%ld: %s: %s/%s\n", (long)getpid(),fileName, path, entry->d_name);
      found++;
    }
  }
  closedir(dir);
  if (!found) {
    printf("nothing found\n");
  }
  return found;
}

void printUsage(char *program_name){
  fprintf(stderr,"Usage: %s [-R] [-i] searchpath filename1 ... filenameN\n", program_name);
}

int main(int argc, char* argv[]){
  int c;    //getopt return value
  const char *options = "Ri";
  char *progName = argv[0];
  int iFlag = 0;
  int rFlag = 0;
  char *searchpath;
  //mit getops holen wir alle Optionen
  //nur flags setzen, und später auswerten
  while((c=getopt(argc, argv, options))!= -1){
      switch(c)
      {
      case 'R':
          rFlag ++;
        break;
      case 'i':
          iFlag ++;
        break;
      case '?':
        fprintf(stderr,"Invalid option, see usage\n");
        printUsage(progName);
        return EXIT_FAILURE;
      default:
        assert(0);
    }
  }

  //mindestens 2 Argumente sind nötig falls nicht -> Abbruch
  if (argc -optind < 2) {
    fprintf(stderr,"ungültige Anzahl der Argumente\n");
    printUsage(progName);
    return EXIT_FAILURE;
  }

  //hier sollen die Optionen dann ausgewertet sein
  //Beim Mehrfachverwendung abbrechen
  if (iFlag > 1 || rFlag > 1) {
    fprintf(stderr, "An option is added multiple times\n");
    printUsage(progName);
    return EXIT_FAILURE;
  }
  if (rFlag ==1) {
    printf("Recursive search is set\n");
  }
  if (iFlag == 1) {
    printf("Ignore capitalization is set\n");
  }

  //optind counts up from 1 - ignores argv[0] - filename
  //Hier wissen wir schon wie viele Suchparameter vorhanden sind
  int searchParameterSize = argc -optind -1;
  char *searchInputs[searchParameterSize];
  searchpath = argv[optind];
  //searchpath soll für Gültigkeit überprüft werden???
  //prüfe ob searchpath existiert - liefert -1 falls Pfad nicht existent, oder keine Berechtigung
  struct stat test;
  int statReturn = stat(searchpath, &test);
  if (statReturn == -1) {
    perror(searchpath);
    return EXIT_FAILURE;
  }
  //prüfe ob das ein Verzeichnis ist
  //in st. mode ist Typ vom file und die Zugriffberechtigungen gespeichert
  if (!S_ISDIR(test.st_mode)) {
    fprintf(stderr, "%s is not a directory\n", searchpath);
    return EXIT_FAILURE;
  }
  //searchpath ist korrekt
  //funkt für relative und absoluter pfad auch
  //realpath gleich im Parent setzen, damit alle forks diese erben
  char *fullpath = realpath(searchpath, NULL);
  printf("The full path is: %s\n", fullpath);

  optind++;
  for (int i = 0; i < searchParameterSize; i++) {
    searchInputs[i] = argv[optind];
    optind++;
  }

  printf("searchpath is %s\n", searchpath );
  printf("searchinputs:\n");
  for (int i = 0; i < searchParameterSize; i++) {
    printf("%d. input: %s \n",i+1, searchInputs[i]);
    searchFile(fullpath, searchInputs[i]);
  }

  free(fullpath);
  return EXIT_SUCCESS;
}
