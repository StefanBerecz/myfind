#include <stdio.h>
//include für getopt()
#include <unistd.h>
#include <assert.h>
#include <sys/stat.h>


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
        return 1;
      default:
        assert(0);
    }
  }

  //mindestens 2 Argumente sind nötig falls nicht -> Abbruch
  if (argc -optind < 2) {
    fprintf(stderr,"ungültiger Anzahl der Argumente\n");
    printUsage(progName);
    return 1;
  }

  //hier sollen die Optionen dann ausgewertet sein
  //Beim Mehrfachverwendung abbrechen
  if (iFlag > 1 || rFlag > 1) {
    fprintf(stderr, "An option is added multiple times\n");
    printUsage(progName);
    return 1;
  }
  if (rFlag ==1) {
    printf("Recursive search ist set\n");
  }
  if (iFlag == 1) {
    printf("Ignore capitalization is set\n");
  }

  //optind counts up from 1 - ignores argv[0] - filename
  //Hier wissen wir schon wie viele Suchparameter vorhanden sind
  int searchParameterSize = argc -optind -1;
  char *searchInputs[searchParameterSize];
  searchpath = argv[optind];
  //searchpath soll überprüft werden???
  struct stat test;
  optind++;
  for (int i = 0; i < searchParameterSize; i++) {
    searchInputs[i] = argv[optind];
    optind++;
  }

  printf("searchpath is %s\n", searchpath );
  printf("searchinputs:\n");
  for (int i = 0; i < searchParameterSize; i++) {
    printf("%s \n", searchInputs[i]);
  }


  return 0;
}
