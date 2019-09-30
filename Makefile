all: main1 main2 main3

main1:  webserver.c server_functions.c threadpool.c
	gcc -g3 -o ./myhttpd webserver.c server_functions.c threadpool.c -lpthread

main2:  webcrawler.c crawler_functions.c threadpool.c 
	gcc -g3 -o ./mycrawler crawler_functions.c threadpool.c webcrawler.c  -lpthread

main3: jobExecutor.c jobexec/functions.c jobexec/parentMessages.c jobexec/childMessages.c jobexec/trie.c jobexec/invindex.c
	gcc -I jobexec/ -g3 -o ./jobExecutor jobExecutor.c jobexec/functions.c jobexec/parentMessages.c jobexec/childMessages.c jobexec/trie.c jobexec/invindex.c

