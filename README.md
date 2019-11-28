# Web-server-client-with-http-requests
Kernel app creates web sites on the disk. Web crawler requests these web sites via http , server is responsible for the transitions of the files in chunks. When transition is done , web crawler is capable for answering queries.

Compilation: make

Execution : 

	./webcreator.sh root_dir file W P 
	./myhttpd -p port -c c_port -t num_threads -d root_directory
	./mycrawler -h host -p port -c c_port -t num_threads -d save_dir starting_URL
Url must be in form: siteX/pageX_Y.html


STATS: 

Crawler sends stats about run time , number of downloaded pages and sum of bytes.
eg:

Crawler up for 01:06.45, downloaded 124 pages, 345539 bytes


SEARCH word1 word2 word3 … word10

If crawler is still in download process informs the user , else crawler responds with the sites that contains those
words most frequently.

SHUTDOWN: Crawler terminates
