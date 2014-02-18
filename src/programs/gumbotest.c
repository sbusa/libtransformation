#include <corenova/source-stub.h>
#include <corenova/data/parser/gumboparser.h>

THIS = {
    .name = "A dummy Tester",
    .version = "0.0",
    .author = "Peter K. Lee <saint@corenova.com>",
    .description = "This program should do nothing.",
    .requires = LIST("corenova.data.parser.gumboparser")    
};

#include <stdio.h>
#include <corenova/sys/getopts.h>
#include <corenova/sys/debug.h>
#include <corenova/data/stree.h>
#include <corenova/data/streams.h>
#include <corenova/data/string.h>
#include <corenova/data/object.h>
#include <corenova/data/database.h>
#include <corenova/net/neticmp.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

static void print_string(const char *str,int pos,int len);
static void read_file(FILE* fp, char** output, int* length);
int main(int argc, char **argv, char **envp) 
{
  printf("program starts\n");
  const char* filename = "input.html";
  FILE* fp = fopen(filename, "r");
  if (!fp)
  {
    printf("File %s not found!\n", filename);
    exit(EXIT_FAILURE);
  }
  char *input;
  int input_length;
  read_file(fp, &input, &input_length);
  printf("length %d file contents are \n%s\n",input_length,input);

  gumboParser_t *myparser =I(GumboParser)->new(input,input_length);
  
  printf("successfully created the new gumbo parser\n");
  
  GumboParserTag mytag=GUMBOPARSER_TAG_A;
  char *at="href";
  char *link="google.com";  
  I(GumboParser)->match(myparser,mytag,at,link);
  I(GumboParser)->remove(myparser);
  
  
  
  char *output=NULL;
  printf("output address %u\n",output);
  output=I(GumboParser)->toString(myparser);
  
  
  printf("output address %u\n",output);
    if(output)
    printf("toString output: %s\n",output);
    
    I(GumboParser)->destroy(myparser);
      printf("program exits\n");
    
    
}


//utility functions

static void print_string(const char *str,int pos,int len)
{
	int i=0;
	printf("print_string ...position (%d) len (%d).\n",pos,len);
	for(i=pos;i<(pos+len);i++)
		printf("%c",*(str+i));
	printf("\n");
}

//This function is referred from the Gumbo example program code- as it is taken.
static void read_file(FILE* fp, char** output, int* length)
{
  static struct stat filestats;
  int fd = fileno(fp);
  fstat(fd, &filestats);
  *length = filestats.st_size;
  *output = malloc(*length + 1);
  int start = 0;
  int bytes_read;
  while ((bytes_read = fread(*output + start, 1, *length - start, fp)))
  {
    start += bytes_read;
  }
}



