shell2: shell2.c
	gcc -o shell2 shell2.c -I.

shell: shell.c
	gcc -o my_shell_EXE shell.c -I .