/* mysh.c
 * Catherine Merz
 * CPSC 405-01
 * Homework #2
 * Create own shell program.
 * 
 * References:
 * www.tutorialspoint.com/c_standard_library/
 * xv6 code (sh.c)
 * www.techonthenet.com/c_language/standard_library_functions/
 * www.gnu.org/software/libc/manual/html_node/
 * man7.org/linux/man-pages/waitpid.2.html
*/

#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <fcntl.h>

void cd(int argslength, char *args[]);
void errormsg();
int hascontent(char string[], int stringsize);
int hasredirection(int argslength, char *args[]);
int isbuiltin(char cmd[]);
char *padchar(char string[], char lookfor[]);

int main(int argc, char *argv[]) {
	// exit if too many args
	if(argc > 2) {
		errormsg();
		exit(0);
	}

	// check where input will be coming from
	int usingfile; // boolean
	if(argc==1)
		usingfile = 0;
	else
		usingfile = 1;

	FILE *inputfile;
	if(usingfile) {

		// get rid of brackets if present
		if(strchr(argv[1],'[') != NULL) 
			strcpy(argv[1], strrchr(argv[1],'[')+1);
		if(strchr(argv[1], ']'))
			argv[1] = strtok(argv[1],"]");
		
		// try to open file
		inputfile = fopen(argv[1], "r");
		if(inputfile == NULL) {
			errormsg();
			exit(0);
		}
	}

	int MAXIN = 520;

	char buf[MAXIN];
	char *args[MAXIN/2];

	args[0] = "";

	while(strcmp(args[0],"exit")) {

		// get input
		if(usingfile) {
			if(fgets(buf, MAXIN, inputfile)==NULL) {
				errormsg();
				exit(0);
			}

			write(STDOUT_FILENO, buf, strlen(buf));

		} else {
			write(STDOUT_FILENO, "mysh> ", 6);
			fgets(buf, MAXIN, stdin);
		}

		// verify not empty string
		int length = strlen(buf);
		if(!(length > 1 && hascontent(buf, length))) {
			continue;
		}
		// make sure input string isn't too long
		if(length > 512) {
			errormsg();
			continue;
		}

		// pad special chars with spaces
//		padchar(buf,">");
//		padchar(buf,"&");

		// pad > with spaces
		if(strstr(buf,">")!=NULL) {
			char newstring[520];
			char *token[10];
			token[0] = strtok(buf,">");
			strcpy(newstring, token[0]);
			while(token[0] != NULL) {
				token[0] = strtok(NULL, ">");
				if(token[0] != NULL) {
					strcat(newstring, " > ");
					strcat(newstring, token[0]);
				} else
					strcat(newstring, "\0");
			}
			strcpy(buf, newstring);
		}

		// pad &s with spaces
		if(strstr(buf,"&")!=NULL) {
			char newstring[520];
			char *token[10];
			token[0] = strtok(buf,"&");
			strcpy(newstring, token[0]);
			while(token[0] != NULL) {
				token[0] = strtok(NULL, "&");
				if(token[0] != NULL) {
					strcat(newstring, " & ");
					strcat(newstring, token[0]);
				} else
					strcat(newstring, "\0");
			}
			strcpy(buf, newstring);
		}


		// parse input, store in args[]
		char delim[] = " \t\n\r"; // space, tab, newline, carriage return
		args[0] = strtok(buf, delim);
		int i = 0;
		while(args[i] != NULL) {
			if(strcmp(args[i], "")) // only increments index if token not empty
				i++;
			args[i] = strtok(NULL, delim);
		}
		int argslength = i; // index of NULL 

//		for(int j=0; args[j] != NULL && j<sizeof(args); j++)
//			printf("args[%d] = %s\n",j,args[j]);

		// do things with input

		// check if exit
		if(strcmp(args[0],"exit")==0) {
			if(usingfile)
				fclose(inputfile);
			exit(0);
		}

		// check for dangling ampersand
		int foundampersand = 0;
		if(argslength > 1 && strcmp(args[argslength-1],"&")==0) {
			foundampersand = 1;
			argslength--;
			args[argslength] = NULL;
		}

		// check for redirection
		int hasred = hasredirection(argslength, args);
		if(hasred>=0 && (hasred==0 || hasred!=(argslength-2))) {
			errormsg();
			continue;
		}

		// check if cd
		if(strcmp(args[0],"cd")==0) {
			cd(argslength,args);
			continue;
		}

		// check if wait
		if(strcmp(args[0],"wait")==0) {
			while(wait(NULL) > 0)   // keep waiting until returns error because no children left
				continue;	
//			int childname = 1;
//			while(childname > 0) { // keep waiting until returns error because no children left
//				childname = wait(NULL);
//				printf("%d\n",childname);
//				continue;
//			}
			continue;
		}

		int rc = fork();

		if(rc != 0) {
			// instructions for parent 
			if(!foundampersand) {
	//			printf("now waiting\n");
				waitpid(rc, NULL, 0);
	//			printf("done waiting\n");
			}

		} else { 
			// instructions for child

			// check for redirection
			if(hasred>=0) {
				close(STDOUT_FILENO);
				open(args[argslength-1], O_CREAT | O_WRONLY | O_TRUNC, S_IRWXU);
				args[hasred] = NULL;
				argslength = hasred;
			}

			// check if pwd
			if(strcmp(args[0],"pwd")==0) {
				char path[200];
				getcwd(path,200);
//				printf("%s\n",path);
				strcat(path,"\n");
				write(STDOUT_FILENO, path, strlen(path));
				exit(0);
			}

			// check if python file
			int isnull = (strstr(args[0],".py")==NULL);
			if(!isnull && (strcmp(strstr(args[0],".py"),".py")==0)) {
	//			printf("This is a python file\n");
				argslength++; // remember, 'argslength' is the index of NULL
				for(int i = argslength; i > 0; i--) {
					args[i] = args[i-1];
				}
				args[0] = "/usr/bin/python";
			}

			// child runs exec
			if(execvp(args[0], args)==-1)
				errormsg();

			exit(0); // child exits (will reach if exec fails)

		} // end of child instructions

	} // end of loop


	return 0;
} // end of main


// functions (in ABC order)

/* Checks whether cd was given additional args and executes cd.
 * @param argslength the number of relevent elements in args[]
 * @param *args[] the array of input tokens
*/
void cd(int argslength, char *args[]) {
	if(argslength == 1) {
		chdir(getenv("HOME"));
	} else {
		chdir(args[1]);
	}
}

void errormsg() {
	char error_message[30] = "An error has occurred\n";
	write(STDERR_FILENO, error_message, strlen(error_message));
}

// Credit to James Murphy for suggesting I make a function that
// checks whether the input is just whitespace.
int hascontent(char string[], int stringsize) {
	int i = 0; 
	while(i < stringsize) {
		if(isalpha(string[i]))
			return 1;
		i++;
	}
	return 0;
}

/* Checks whether output is being redirected.
 * @param argslength the number of relevent elements in args[] (the index of NULL)
 * @param *args[] the array of input tokens
 * @return -1 if not found, else index of args[] where found
*/
int hasredirection(int argslength, char *args[]) {
	int found = 0;
	int index;
	for(int i=0; (i<argslength) && !found; i++) {
		if(strcmp(args[i],">")==0) {
			found = 1;
			index = i;
		}
	}
	if(!found)
		return -1;
	return index;
}

/* Checks whether input is calling a built in function.
 * @param cmd[] the first token of input (args[0])
 * @return an int indicating which function was called, 0 if invalid
*/
int isbuiltin(char cmd[]) {
	if(strcmp(cmd,"exit")==0)
		return 1;
	if(strcmp(cmd,"cd")==0)
		return 2;
	if(strcmp(cmd,"pwd")==0)
		return 3;
	if(strcmp(cmd,"wait")==0)
		return 4;
	return 0;
}

/* Adds spaces around instances of the specified character.
 * @param string[] the string to look inside
 * @param lookfor the character to add spaces around
 * @return a copy of string with spaces added to either side at all
 * 	occurrances of the given character
*/
char *padchar(char string[], char lookfor[]) {
	printf("called function\n");
	if(strstr(string,lookfor)==NULL)
		return string;
	char newstring[520];
	strcpy(newstring, string);
	printf("strcpy worked, newstring = '%s'\n", newstring);
	char *token[1];
	token[0] = strtok(newstring,lookfor);
	printf("token set up, token = %s\n", token[0]);
	strcpy(string, token[0]);
	printf("strcpy worked, string = '%s'\n", string);
	printf("token = %s\n", token[0]);
	printf("string = %s\n", string);
	printf("about to try to go into while loop: ");			
	while(token[0] != NULL) {
		printf("got into while loop");
		strcat(string, " ");
		strcat(string, lookfor);
		strcat(string, " ");
		token[0] = strtok(NULL, lookfor);
		strcat(string, token[0]);
	}
	printf("out of while loop\n");
	return string;
}
