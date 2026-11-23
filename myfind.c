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
//für strcasecmp
#include <strings.h>
//für PATH_MAX und NAME_MAX
#include <limits.h>
//für fork(), waitpid() und pid_t
#include <sys/types.h>
#include <sys/wait.h>


int searchFile(const char *path, const char *fileName, int iFlag, int rFlag, int outFd) {
  int found = 0;
  struct dirent *entry;
  DIR *dir = opendir(path);
  //error handling on opendir
  if (dir == NULL) {
    perror(path);
    return -1;
  }

  while ((entry = readdir(dir)) != NULL) {
    //kompletter Pfad für die Rückgabe/Rekursion
    char entryPath[PATH_MAX];
    snprintf(entryPath, sizeof(entryPath), "%s/%s", path, entry->d_name);
    //Eintrag für selbst, und parent-Ordner überspringen
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
      continue;
    }
    if (rFlag) {
      //Prüfen ob entry ein Ordner ist

      if (entry->d_type == DT_DIR) {
        //Treffer aus Subfoldern addieren, -1 (Ordner nicht lesbar) ignorieren
        int subFound = searchFile(entryPath, fileName, iFlag, rFlag, outFd);
        if (subFound > 0) {
          found += subFound;
        }
      }
    }
    //iFlag wird zuerst ausgewertet
    //Falls ein Vergleich gefunden wird 0 == 0 --> 1
    int match = iFlag == 1 ? strcasecmp(entry->d_name, fileName) == 0 : strcmp(entry->d_name, fileName) == 0;
    if (match) {
      //maximal pfadlänge + maximale Namenslänge + 32 Stellen für pid und filler Characters bei der Ausgabe
      char result[PATH_MAX+NAME_MAX+32];
      int len = snprintf(result, sizeof(result), "%ld: %s: %s\n", (long)getpid(),fileName, entryPath);
      //ganze Zeile mit write() in die Pipe schreiben
      if (write(outFd, result, len) == -1) {
        perror("write");
      }
      found++;
    }
  }
  closedir(dir);
  return found;
}

void printUsage(char *program_name){
  fprintf(stderr,"Usage: %s [-R] [-i] searchpath filename1 ... filenameN\n", program_name);
}

/*
Parallelisierung und synchronisierte Ausgabe:

Der Parent legt vor dem Forken eine unnamed pipe an und startet für jeden
Dateinamen einen Child-Prozess mit fork(). Die Childs suchen parallel und
erben die Pipe. Jede Ergebniszeile "<pid>: <filename>: <pfad>\n" wird im
Child vollständig in einem Puffer aufgebaut und mit genau einem write() in
das Schreibende der Pipe geschrieben.

Nur der Parent schreibt auf stdout: Er liest die Pipe bis EOF und gibt die
Daten unverändert aus. Damit gibt es auf stdout nur einen Schreiber, die
Zeilen bleiben vollständig lesbar, ihre Reihenfolge ist unsortiert.

Der Parent liest die Pipe aus, bevor er wartet und sammelt danach alle
Childs mit waitpid() ein, damit keine Zombie-Prozesse entstehen.
*/
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

  //optind counts up from 1 - ignores argv[0] - filename
  //Hier wissen wir schon wie viele Suchparameter vorhanden sind
  int searchParameterSize = argc -optind -1;
  char *searchInputs[searchParameterSize];
  searchpath = argv[optind];
  //searchpath soll für Gültigkeit überprüft werden???
  //prüfe ob searchpath existiert - liefert -1 falls Pfad nicht existent, oder keine Berechtigung
  struct stat searchpathStat;
  int statReturn = stat(searchpath, &searchpathStat);
  if (statReturn == -1) {
    perror(searchpath);
    return EXIT_FAILURE;
  }
  //prüfe ob das ein Verzeichnis ist
  //in st. mode ist Typ vom file und die Zugriffberechtigungen gespeichert
  if (!S_ISDIR(searchpathStat.st_mode)) {
    fprintf(stderr, "%s is not a directory\n", searchpath);
    return EXIT_FAILURE;
  }
  //searchpath ist korrekt
  //funkt für relative und absoluter pfad auch
  //realpath gleich im Parent setzen, damit alle forks diese erben
  char *fullpath = realpath(searchpath, NULL);
  if (fullpath == NULL) {
    perror(searchpath);
    return EXIT_FAILURE;
  }

  optind++;
  for (int i = 0; i < searchParameterSize; i++) {
    searchInputs[i] = argv[optind];
    optind++;
  }

  //pipe vor den forks anlegen, damit alle Childs sie erben
  //fd[0] = Leseende, fd[1] = Schreibende
  int fd[2];
  if (pipe(fd) == -1) {
    perror("pipe");
    free(fullpath);
    return EXIT_FAILURE;
  }

  //stdout-Puffer leeren, sonst erbt jeder Child den Inhalt und gibt ihn doppelt aus
  fflush(stdout);

  //für jeden Suchbegriff einen Child-Prozess starten
  int childCount = 0;
  for (int i = 0; i < searchParameterSize; i++) {
    pid_t pid = fork();
    if (pid == -1) {
      //keine weiteren Childs starten, bereits gestartete aber noch einsammeln
      perror("fork");
      break;
    }
    if (pid == 0) {
      //Child: Leseende wird nicht gebraucht
      close(fd[0]);
      int found = searchFile(fullpath, searchInputs[i], iFlag, rFlag, fd[1]);
      close(fd[1]);
      free(fullpath);
      //Child muss hier enden, sonst forkt er in der Schleife selbst weiter
      exit(found < 0 ? EXIT_FAILURE : EXIT_SUCCESS);
    }
    //Parent: zählen, wie viele Childs laufen
    childCount++;
  }

  //Parent: eigenes Schreibende schließen, sonst liefert read() nie EOF
  close(fd[1]);

  //Pipe bis EOF lesen und auf stdout ausgeben
  //EOF kommt erst, wenn alle Childs ihr Schreibende geschlossen haben
  char buffer[4096];
  ssize_t bytesRead;
  while ((bytesRead = read(fd[0], buffer, sizeof(buffer))) > 0) {
    if (write(STDOUT_FILENO, buffer, bytesRead) == -1) {
      perror("write stdout");
      break;
    }
  }
  if (bytesRead == -1) {
    perror("read");
  }
  close(fd[0]);

  //alle Childs einsammeln -> keine Zombies
  int exitCode = EXIT_SUCCESS;
  while (childCount > 0) {
    int status;
    if (waitpid(-1, &status, 0) == -1) {
      perror("waitpid");
      exitCode = EXIT_FAILURE;
      break;
    }
    childCount--;
    if (!WIFEXITED(status) || WEXITSTATUS(status) != EXIT_SUCCESS) {
      exitCode = EXIT_FAILURE;
    }
  }

  free(fullpath);
  return exitCode;
}
