/***********************************************************************

Filename: fsaccess.c
Team Members: Nidhin Anisham

Use command "gcc fsaccess.c" to compile
Use command "./a.out" to run
 
This program allows user to do two things: 
 1. initfs: Initilizes the file system and redesigning the Unix file system to accept large 
    files of up tp 4GB, expands the free array to 152 elements, expands the i-node array to 
    200 elemnts, doubles the i-node size to 64 bytes and other new features as well.
 2. Quit: save all work and exit the program.
 
User Input:
   - initfs (file path) (# of total system blocks) (# of System i-nodes)
   - q
   
File name is limited to 14 characters.



Errors found:
1. Superblock was 1028 bytes. Restructured superblock by placing the free array at the end
2. In the validation present in preInitialization(), there was a lack of brackets while the file descriptor was being initialized.

***********************************************************************/

#include<stdio.h>
#include<fcntl.h>
#include<unistd.h>
#include<errno.h>
#include<string.h>
#include<stdlib.h>
#include <sys/stat.h>

#define FREE_SIZE 152  
#define I_SIZE 200
#define BLOCK_SIZE 1024    
#define ADDR_SIZE 11
#define INPUT_SIZE 256


// Superblock Structure
// **************************************** The order of datatypes was incorrect ***********************************
typedef struct {
  unsigned short isize;             // The no of blocks with iNodes
  unsigned short fsize;             // The total no of data blocks
  unsigned short nfree;             // The no of free data nodes in the free array
  unsigned short ninode;            // The no of free iNodes present in the iNode array
  unsigned short inode[I_SIZE];     // The array containing all the free inode numbers
  char flock;
  char ilock;
  unsigned short fmod;
  unsigned short time[2];
  unsigned int free[FREE_SIZE];     // The array containing all the free data nodes
} superblock_type;

superblock_type superBlock;

// I-Node Structure

typedef struct {
unsigned short flags;               // Contains all the flags related to allocation, large/small file, read, write and execution rights to users, groups and others 
unsigned short nlinks;
unsigned short uid;
unsigned short gid;
unsigned int size;                  // Contains the size of the file
unsigned int addr[ADDR_SIZE];       // Contains the locations of all the blocks which contain the file contents
unsigned short actime[2];
unsigned short modtime[2];
} inode_type; 

inode_type inode;

//Directory Structure

typedef struct {
  unsigned short inode;             // Reference to the iNode number of the file/directory
  unsigned char filename[14];       // name of the file or the directory
} dir_type;

dir_type root;
dir_type temp;

int fileDescriptor ;		//file descriptor 
short currentInode;
const unsigned short inode_alloc_flag = 0100000;      // INode flag to tell that the iNode is allocated
const unsigned short dir_flag = 0040000;               // Flag to determine if it is a directory
const unsigned short dir_large_file = 0010000;         // Flag to determine if it is a large/small file
const unsigned short dir_access_rights = 0000777; // User, Group, & World have all access privileges 
const unsigned short INODE_SIZE = 64; // inode has been doubled

//function declaration
int initfs(char* path, unsigned short total_blcks,unsigned short total_inodes);
void add_block_to_free_list( int blocknumber , unsigned int *empty_buffer );
void create_root();
void rm_file_using_inumber(short i_number_directory, short i_number_file);
int check_if_dir(short cur_node);
char* pwd_recurse(short i_no,char* result);
char* concat(const char *s1, const char *s2);
void rm_folder_using_inumber(short i_number);
void in_plainfile(unsigned int size, int ef, char* v6file);
void out_plainfile(unsigned int size,int ef, inode_type temp_inode);
short get_inode_by_file_name(char* fileName);
void update_current_inode(short i_no);

int main() {
 
  char input[INPUT_SIZE];
  char *splitter;
  unsigned int numBlocks = 0, numInodes = 0;
  printf("Size of super block = %d , size of i-node = %d\n",sizeof(superBlock),sizeof(inode));
  printf("Enter command:\n");
  
  // While(1) keep on looking for commands being input by the user
  while(1) {
  
    scanf(" %[^\n]s", input);
    splitter = strtok(input," "); //get the input command
    
	  //block to process initfs command
    if(strcmp(splitter, "initfs") == 0){
    
        preInitialization();
        splitter = NULL;                   
    } 
	
	else if(strcmp(splitter,"cpin") == 0){
		cpin();
		splitter = NULL;
	}
	
	else if(strcmp(splitter,"cpout") == 0){
		cpout();
		splitter = NULL;
	}
	
	else if(strcmp(splitter,"mkdir") == 0){
		mk_dir();
		splitter = NULL;
	}
	else if(strcmp(splitter,"ls") == 0){
		ls();
		splitter = NULL;
	}
	
	else if(strcmp(splitter,"rm") == 0){
		rm();
		splitter = NULL;
	}
	else if(strcmp(splitter,"rmdir") == 0){
		rm_dir();
		splitter = NULL;
	}
	else if(strcmp(splitter,"open") == 0){
		char* v6name = strtok(NULL," ");
		if(!v6name){
			printf("File name not given\n\n");
		}
		else{
			open_v6(v6name);
		}
		splitter = NULL;
	}
	
	else if(strcmp(splitter,"print") == 0){
		printf("Free blocks: %d\n",superBlock.nfree);
		printf("Free inodes: %d\n",superBlock.ninode);
		printf("Current inode: %d\n",currentInode);
		int i;
		printf("Blocks in current inode: ");
		for(i = 0;i<ADDR_SIZE;i++){
			printf("%d  ",inode.addr[i]);
		}
		printf("\n\n");
		splitter = NULL;
	}
	
	else if(strcmp(splitter,"print_free") == 0){
		int i;
		while(i!=-1){
			i = get_free_data_block();
			printf("%d ",i);
		}
		printf("\n\n");
		splitter = NULL;
	}
	
	else if(strcmp(splitter,"cd") == 0){
		char* cd_path = strtok(NULL," ");
		if(!cd_path){
			printf("Path not given");
		}
		else{
			change_dir(cd_path);
		}
		splitter = NULL;
	}
	
	else if(strcmp(splitter,"pwd") == 0){
		present_working_directory();
		splitter = NULL;
	}
	
	
	//block to process quit command
	else if (strcmp(splitter, "q") == 0) {
      lseek(fileDescriptor, BLOCK_SIZE, 0); //Seek to super block
      write(fileDescriptor, &superBlock, BLOCK_SIZE);  //Save super block before quitting
      return 0;
    } 
  }
}

//Function for processing input and validation of the input
int preInitialization(){

  char *filepath;
  char *n1, *n2;
  unsigned int numBlocks = 0, numInodes = 0;
  
  filepath = strtok(NULL, " ");
  n1 = strtok(NULL, " ");
  n2 = strtok(NULL, " ");
         
  
  //Check if file exists
  if(access(filepath, F_OK) != -1) {
	  open_v6(filepath);
      printf("Filesystem already exists and the same will be used.\n\n");
	  
  } else {
			
			//check if number of inodes and number of blocks are entered
    	if (!n1 || !n2) {
          printf("All arguments(path, number of inodes and total number of blocks) have not been entered\n");
      } else {
      		numBlocks = atoi(n1);
      		numInodes = atoi(n2);
		      //Call initfs function
      		if( initfs(filepath,numBlocks, numInodes )){
      		  printf("The file system is initialized\n\n");
      		} else {
        		printf("Error initializing file system. Exiting... \n\n");
      		}
   		}
  }
  return 1;
}

int initfs(char* path, unsigned short blocks,unsigned short inodes) {

   unsigned int buffer[BLOCK_SIZE/4]; //Used for setting the block to 0. Int = 4 bytes thus we require a buffer of size 1024/4
   int bytes_written;
   
   
   unsigned short i = 0;
   superBlock.fsize = blocks;
   unsigned short inodes_per_block= BLOCK_SIZE/INODE_SIZE; //number of inodes per block = 1024/64 = 16
   
   //set number of i node blocks in isize. If inodes is not a multiple of inodes_per_block, assign one extra block to store the remainder inodes
   if((inodes%inodes_per_block) == 0)
		superBlock.isize = inodes/inodes_per_block;
   else
		superBlock.isize = (inodes/inodes_per_block) + 1;
   
   if((fileDescriptor = open(path,O_RDWR|O_CREAT,0700))== -1)
       {
           printf("open() failed with the following error [%s]\n\n",strerror(errno));
           return 0;
       }
       
   for (i = 0; i < FREE_SIZE; i++)
      superBlock.free[i] =  0;			//initializing free array to 0 to remove junk data. free array will be stored with data block numbers shortly.
  

   // Initially the number of free nodes are set to zero and number of iNodes is set to I_SIZE
   superBlock.nfree = 0;
   if(inodes > I_SIZE){
	   superBlock.ninode = I_SIZE;
   }
   else{
	   superBlock.ninode = inodes;
   }
   
   
   // We can directly start putting the iNodes into the inode array from the start as there are no files initially
   for (i = 0; i < superBlock.ninode; i++)
	    superBlock.inode[i] = i + 1;		//Initializing the inode array to inode numbers
   
   superBlock.flock = 'a'; 					//flock,ilock and fmode are not used.
   superBlock.ilock = 'b';					
   superBlock.fmod = 0;
   superBlock.time[0] = 0;
   superBlock.time[1] = 1970;
   
   lseek(fileDescriptor, BLOCK_SIZE, SEEK_SET); //seek to end of boot block
   write(fileDescriptor, &superBlock, BLOCK_SIZE); // writing superblock to file system
   
   for (i = 0; i < BLOCK_SIZE/4; i++) 
   	  buffer[i] = 0;
        
   for (i = 0; i < superBlock.isize; i++)
   	  write(fileDescriptor, buffer, BLOCK_SIZE); //initializing all inode blocks with 0
   
   // Create root directory
   create_root();
   //loop starts from block after inode blocks
   for ( i = 2 + superBlock.isize + 1; i < blocks; i++ ) {
      add_block_to_free_list(i , buffer); //add the data blocks to free in super block
   }
   return 1;
}

// Add Data blocks to free list
void add_block_to_free_list(int block_number,  unsigned int *empty_buffer){

  //if free blocks are filled, assign free blocks in an another data block and create a chain
  if ( superBlock.nfree == FREE_SIZE ) { 

    int free_list_data[BLOCK_SIZE / 4], i;
    free_list_data[0] = FREE_SIZE;  
    
    for ( i = 0; i < BLOCK_SIZE / 4; i++ ) {
       if ( i < FREE_SIZE ) {
         free_list_data[i + 1] = superBlock.free[i]; //copying data of free array in super block to free_list_data
       } else {
         free_list_data[i + 1] = 0; // getting rid of junk data in the remaining unused bytes of header block
       }
    }
    
    lseek( fileDescriptor, block_number * BLOCK_SIZE, 0 );
    write( fileDescriptor, free_list_data, BLOCK_SIZE ); // Writing free list to block = block_number
    
    superBlock.nfree = 0; //clear out free array in superblock
    
  } else {

	  lseek( fileDescriptor, block_number * BLOCK_SIZE, 0 );
	  write( fileDescriptor, empty_buffer, BLOCK_SIZE );  // writing 0 to remaining data blocks to get rid of junk data
  }

  superBlock.free[superBlock.nfree] = block_number;  // Assigning blocks to free array
  ++superBlock.nfree;
}

// Create root directory
void create_root() {

	int root_data_block = 2 + superBlock.isize; // Allocating first data block to root directory
	int i;
  
	root.inode = 1;   // root directory's inode number is 1.
	currentInode = 1;
	root.filename[0] = '.';
	root.filename[1] = '\0';
  
	//setting inode for root directory
	inode.flags = inode_alloc_flag | dir_flag | dir_large_file | dir_access_rights;   		// flag for root directory 
	inode.nlinks = 0; 
	inode.uid = 0;
	inode.gid = 0;
	inode.size = INODE_SIZE;
	inode.addr[0] = root_data_block;
	//adress block will point to 0
	for( i = 1; i < ADDR_SIZE; i++ ) {
		inode.addr[i] = 0;
	}
  
	inode.actime[0] = 0;
	inode.modtime[0] = 0;
	inode.modtime[1] = 0;
  
	lseek(fileDescriptor, 2 * BLOCK_SIZE, 0);
	write(fileDescriptor, &inode, INODE_SIZE);   //writing to root directory inode i.e inode 1	
	lseek(fileDescriptor, BLOCK_SIZE*root_data_block, 0);
	write(fileDescriptor, &root, 16);  //write 16 bytes of header
			    
	// .. represents directory
	root.filename[0] = '.';
	root.filename[1] = '.';
	root.filename[2] = '\0';
	write(fileDescriptor, &root, 16); //14 bytes for filename and 2 bytes for inode number
	/*
	int k;
	for(k = 0;k<BLOCK_SIZE/16;k++){
		lseek(fileDescriptor,BLOCK_SIZE*(inode.addr[0])+16*k,SEEK_SET);
		read(fileDescriptor,&temp,16);
		printf("%d  %s\n",temp.inode,temp.filename);
	}
	*/

}

int get_free_data_block() {
    int block; 
    superBlock.nfree--;    
	block = superBlock.free[superBlock.nfree];	
	
    if (superBlock.nfree == 0){
		
        int chain_block = superBlock.free[0];
		printf("Chain Block is %d!\n",chain_block);
        if(chain_block == 2 + superBlock.isize + 1){
            printf("No free blocks");
            return -1;
        }
        else{
			int i;
			int free_block_nos;
			lseek(fileDescriptor,BLOCK_SIZE*chain_block,0);
			read(fileDescriptor,&free_block_nos,sizeof(int));
			//lseek(fileDescriptor,(BLOCK_SIZE*chain_block)+sizeof(int),0);
			for(i=0;i<free_block_nos;i++){
				int free_block;
				read(fileDescriptor, &free_block, sizeof(int));
				superBlock.free[superBlock.nfree] = free_block;
				superBlock.nfree++;				
			}
        }
    }
    return block;
}

char* parse_path(char* pathname){
	char* token[INPUT_SIZE];
	if(!pathname){
		return NULL;
	}
	if(pathname[0] == '/'){    
        currentInode = 1;                
    } 
	short cur_node,i=0;
    token[i] = strtok(pathname, "/");
    while (token[i] != NULL) {      
		i++;
        token[i] = strtok(NULL, "/");
		if(token[i] == NULL){
			return token[i-1];
		}
		else{
			cur_node = get_inode_by_file_name(token[i-1]);
			if(cur_node == -1) {
				
				return NULL;
			}
			else if(check_if_dir(cur_node)){
				currentInode = cur_node;
				lseek(fileDescriptor,2*BLOCK_SIZE+(currentInode-1)*INODE_SIZE,0);
				read(fileDescriptor,&inode,INODE_SIZE);
			}
			else{
				printf("Invalid path given!\n\n");
				return NULL;
			}
		}
    }
    return NULL;
}

unsigned short get_free_inode(){
    int inodes;
    int i = 0;
	superBlock.ninode--;
	if(superBlock.ninode == 0){
		superBlock.ninode = 1;
		inode_type temp_inode;
		inodes = superBlock.isize*(BLOCK_SIZE/INODE_SIZE); 
		for(i=I_SIZE+1;i<inodes;i++){
			lseek(fileDescriptor,BLOCK_SIZE*2+INODE_SIZE*(i-1),SEEK_SET);
			read(fileDescriptor, &temp_inode, INODE_SIZE);
			if((temp_inode.flags & inode_alloc_flag) != inode_alloc_flag){
				return i;
			}
		}
		return -1;
	}
	else{
		return superBlock.inode[superBlock.ninode];
	}
    return -1;
}

short get_inode_by_file_name(char* fileName) {
	dir_type block;
	int i, j;
	for(i=0; i<ADDR_SIZE; i++) {
		if(inode.addr[i]!=0){
			for(j=0; j<(BLOCK_SIZE/16); j++) {
				lseek(fileDescriptor,BLOCK_SIZE*inode.addr[i]+16*j,SEEK_SET);
				read(fileDescriptor,&block,16);
				if(strcmp(block.filename, fileName)==0) {
					return block.inode;
				}
			}
		}
	}
	return -1;
}

void update_current_inode(short i_no){
	lseek(fileDescriptor,2*BLOCK_SIZE+(i_no-1)*INODE_SIZE,0);
	read(fileDescriptor,&inode,INODE_SIZE);
	currentInode = i_no;
}

int cpout() {
	char *externalfile;
	char *v6file;
	int size, ef;
	FILE* externfile;
	short i_number;
	v6file = strtok(NULL, " ");
	externalfile = strtok(NULL, " ");
	short temp_currentInode = currentInode;
	v6file = parse_path(v6file);
	inode_type temp_inode;
	//Check if file exists
	if(!v6file){
		printf("Invalid internal file path\n\n");
	} else {
		externfile = fopen(externalfile, "w");
		if((ef = open(externalfile, O_RDWR, 0600)) == -1){
			printf("open() failed with error [%s]\n\n", strerror(errno));
			update_current_inode(temp_currentInode);
			return 1;
		}
		
		i_number = get_inode_by_file_name(v6file);
		if(i_number==-1) {
			printf("File not found in the v6 file system!\n\n");
			update_current_inode(temp_currentInode);
			return -1;
		}
		lseek(fileDescriptor,BLOCK_SIZE*2+INODE_SIZE*(i_number-1),SEEK_SET);
		read(fileDescriptor, &temp_inode, INODE_SIZE);
		unsigned int size = temp_inode.size;
		if (size < 11264){
			out_plainfile(size,ef,temp_inode);
		} 
		else{
			printf("Large file\n\n");
		}
		update_current_inode(temp_currentInode);
		return 0;
	}
	update_current_inode(temp_currentInode);
	return 0;
}

void out_plainfile(unsigned int size,int ef, inode_type temp_inode){
	int i;
	for(i=0; i<ADDR_SIZE; i++){
			if(temp_inode.addr[i]==0) {
				break;
			}
			char bf[BLOCK_SIZE] = "";
			if(size > BLOCK_SIZE){
				lseek(fileDescriptor, BLOCK_SIZE*temp_inode.addr[i], SEEK_SET);
				read(fileDescriptor, &bf , BLOCK_SIZE);
				lseek(ef , BLOCK_SIZE*i , SEEK_SET);
				write(ef ,&bf , BLOCK_SIZE);
			}
			else{
				lseek(fileDescriptor, BLOCK_SIZE*temp_inode.addr[i], SEEK_SET);
				read(fileDescriptor, &bf , size);
				lseek(ef , BLOCK_SIZE*i , SEEK_SET);
				write(ef ,&bf , size);
			}
			size = size - BLOCK_SIZE;
		}
		printf("File has been copied out\n\n");
}

int update_dir(dir_type dir){
	unsigned short addrcount = 0;
	int i=0;
	for (addrcount=0;addrcount < ADDR_SIZE; addrcount++)			
	{	
		if(inode.addr[addrcount] == 0){	
			inode.addr[addrcount] = get_free_data_block();
		}
		for (i=0;i<BLOCK_SIZE/16;i++)										
		{		
			lseek(fileDescriptor,BLOCK_SIZE*inode.addr[addrcount]+16*i,SEEK_SET);
			read(fileDescriptor,&temp,16);
			if(temp.inode == 0)
			{
				lseek(fileDescriptor,BLOCK_SIZE*inode.addr[addrcount]+16*i,SEEK_SET);
				write(fileDescriptor,&dir,16);
				/*
				int k;
				for(k = 0;k<BLOCK_SIZE/16;k++){
					//printf("%d\n",k);
					lseek(fileDescriptor,BLOCK_SIZE*(inode.addr[0])+16*k,SEEK_SET);
					read(fileDescriptor,&temp,16);
					printf("%d  %s\n",temp.inode,temp.filename);
				}
				*/
				return 0;
			}
		}
	}
	printf("Error writing directory!");
	return 0;
}

int write_inode(short i_no, inode_type inode_w){
	lseek(fileDescriptor,BLOCK_SIZE*2+INODE_SIZE*(i_no - 1),SEEK_SET);
	write(fileDescriptor,&inode_w,INODE_SIZE);
}


int cpin(){
	char *externalfile ="";
	char *v6file = "";
	FILE* externfile;
    externalfile = strtok(NULL, " ");
	v6file = strtok(NULL, " ");
	short temp_currentInode = currentInode;
	v6file = parse_path(v6file);
	
	int ef;
	unsigned int size;
         
	//Check if file exists
	if(access(externalfile, F_OK) == -1) {
		printf("External file does not exist\n\n");
	} 
	else {
		if((ef = open(externalfile, O_RDWR, 0600)) == -1){
			printf("open() failed with error [%s]\n\n", strerror(errno));
		}
		if(!v6file){
			printf("Invalid path!\n\n");
		}
		else if(get_inode_by_file_name(v6file) != -1){
			printf("File with same name exists. Please give a different name\n\n");
		}
		else{
			externfile = fopen(externalfile,"r");
			struct stat st;
			stat(externalfile, &st);
			size = st.st_size;
			if (size < 11264){
				in_plainfile(size,ef,v6file);
			} 
			else{
				printf("Large file\n\n");
			}
		}
	}
	update_current_inode(temp_currentInode);
	return 0;
}

void in_plainfile(unsigned int size, int ef, char* v6file){
	int no_of_blocks = (size/BLOCK_SIZE)+1;
	int i_number = get_free_inode();
	int i;
	dir_type dir;
	dir.inode = i_number;
	memcpy(dir.filename,v6file,strlen(v6file));
	inode_type temp_inode;
	temp_inode.flags = inode_alloc_flag | 000000 | dir_access_rights;
	temp_inode.nlinks = 1; 
	temp_inode.uid = '0';
	temp_inode.gid = '0';
	temp_inode.size = size;
	temp_inode.actime[0] = 0;
	temp_inode.modtime[0] = 0;
	temp_inode.modtime[1] = 0;
	for(i=0; i<no_of_blocks; i++){
		char bf[BLOCK_SIZE] = "";
		temp_inode.addr[i] = get_free_data_block();
		lseek(ef , BLOCK_SIZE*i , SEEK_SET);
		read(ef ,&bf , BLOCK_SIZE);
		lseek(fileDescriptor , BLOCK_SIZE*temp_inode.addr[i] , SEEK_SET);
		write(fileDescriptor , &bf , BLOCK_SIZE);
		lseek(fileDescriptor , BLOCK_SIZE*temp_inode.addr[i] , SEEK_SET);
		read(fileDescriptor , &bf , BLOCK_SIZE);
	}
	for(i = no_of_blocks;i<ADDR_SIZE;i++){
		temp_inode.addr[i] = 0;
	}
	write_inode(i_number,temp_inode);		
	update_dir(dir);
	printf("File has been copied in\n\n");	
}

int mk_dir(){
	short temp_currentInode = currentInode;
	char* dir_name = strtok(NULL," ");
	dir_name = parse_path(dir_name);
	if(!dir_name){
		printf("Directory name invalid!\n\n");
	}
	else if(get_inode_by_file_name(dir_name) != -1){
		printf("Directory with same name exists. Please give a different name!\n\n");
	}
	else{
		int dir_data_block = get_free_data_block(); 
		int i;
		short free_i = get_free_inode();
		dir_type dir;
		dir.inode = free_i;
		dir.filename[0] = '.';
		dir.filename[1] = '\0';
		inode_type temp_inode;
		temp_inode.flags = inode_alloc_flag | dir_flag | dir_large_file | dir_access_rights;   		
		temp_inode.nlinks = 0; 
		temp_inode.uid = 0;
		temp_inode.gid = 0;
		temp_inode.size = INODE_SIZE;
		temp_inode.addr[0] = dir_data_block;
		//adress block will point to 0
		for( i = 1; i < ADDR_SIZE; i++ ) {
			temp_inode.addr[i] = 0;
		}
  
		temp_inode.actime[0] = 0;
		temp_inode.modtime[0] = 0;
		temp_inode.modtime[1] = 0;
		write_inode(dir.inode,temp_inode);
  
		lseek(fileDescriptor, BLOCK_SIZE*dir_data_block, 0);
		write(fileDescriptor, &dir, 16);  //write 16 bytes of header
  
		// .. represents directory
		dir.inode = currentInode;
		dir.filename[0] = '.';
		dir.filename[1] = '.';
		dir.filename[2] = '\0';
  
		write(fileDescriptor, &dir, 16); //14 bytes for filename and 2 bytes for inode number
		dir.inode = free_i;
		memcpy(dir.filename,dir_name,strlen(dir_name));
		update_dir(dir);
		printf("Directory created\n\n");
	}
	update_current_inode(temp_currentInode);	
	return 0;
}


int open_v6(char* filepath){
	
	if((fileDescriptor = open(filepath, O_RDWR, 0600)) == -1){
		printf("\nopen() failed with error [%s]\n\n", strerror(errno));
		return 1;
	}
	currentInode = 1;
	lseek(fileDescriptor, BLOCK_SIZE, SEEK_SET); //seek to end of boot block
    read(fileDescriptor, &superBlock, BLOCK_SIZE); // reading superblock
	lseek(fileDescriptor, BLOCK_SIZE*2, SEEK_SET); //seek to end of superblock
    read(fileDescriptor, &inode, INODE_SIZE); // reading inode
	printf("Filesystem opened successfully\n\n");
	return 0;
}

void add_inode_to_inode_list(short i_number) {
	int i;
	//Used for setting the block to 0. Int = 4 bytes thus we require a buffer of size 1024/4
	unsigned int buffer[INODE_SIZE/4];
	for(i=0; i<INODE_SIZE/4; i++) {
		buffer[i] = 0;
	}
	int min_inodes = I_SIZE;
	if(min_inodes > superBlock.isize*(BLOCK_SIZE/INODE_SIZE)){
		min_inodes = superBlock.isize*(BLOCK_SIZE/INODE_SIZE);
	}
	if(superBlock.ninode < min_inodes) {
		superBlock.ninode++;
		superBlock.inode[superBlock.ninode] = i_number;
	}
	lseek(fileDescriptor, 2*BLOCK_SIZE + (i_number-1)*INODE_SIZE, SEEK_SET);
	write(fileDescriptor, &buffer, INODE_SIZE);
}

int rm() {
	short temp_currentInode = currentInode;
	char* fileName = strtok(NULL," ");
	fileName = parse_path(fileName);
	// Using directory decoding, cd to that directory

	// Check if the file to be deleted is a directory or a file

	// Pass the inode number of the file to be deleted and the directory
	
	short i_number = get_inode_by_file_name(fileName);
	if(i_number==-1) {
		printf("File not present in directory!\n\n");
	} else {
		// Check if the file to be deleted is a directory or a file
		if(check_if_dir(i_number)) {
			printf("The input given is a directory!\n");
		} else {
			rm_file_using_inumber(currentInode, i_number);
		}
	}
	update_current_inode(temp_currentInode);
	return 0;
}

void rm_file_using_inumber(short i_number_directory, short i_number_file) {
	int i, j;
	dir_type block;

	//Used for setting the block to 0. Int = 4 bytes thus we require a buffer of size 1024/4
	unsigned int buffer[BLOCK_SIZE/4];

	for(i=0; i<BLOCK_SIZE/4; i++) {
		buffer[i] = 0;
	}
	inode_type temp_inode;
	// Deleting the file contents and adding the blocks to the free list
	lseek(fileDescriptor , 2*BLOCK_SIZE + (i_number_file-1)*INODE_SIZE , SEEK_SET);
	read(fileDescriptor, &temp_inode, INODE_SIZE);
	for(i=0; i<ADDR_SIZE; i++) {
		if(temp_inode.addr[i] == 0) {
			break;
		}
		lseek(fileDescriptor, BLOCK_SIZE*(temp_inode.addr[i]), SEEK_SET);
		write(fileDescriptor, &buffer, BLOCK_SIZE);
		//printf("Block %d added to the free list.\n", temp_inode.addr[i]);
		add_block_to_free_list(temp_inode.addr[i], buffer);
		temp_inode.addr[i] = 0;
	}

	// Deleting entry from the directory
	lseek(fileDescriptor, 2*BLOCK_SIZE + (i_number_directory-1)*INODE_SIZE, SEEK_SET);
	read(fileDescriptor, &temp_inode, INODE_SIZE);
	int end_j = BLOCK_SIZE/16;
	int end_addr_index;
	int deletion_j;
	int deletion_addr_index;
	for(i=0; i<ADDR_SIZE; i++) {
		for(j=0; j<(BLOCK_SIZE/16); j++) {
			lseek(fileDescriptor, (temp_inode.addr[i])*BLOCK_SIZE + j*16, SEEK_SET);
			read(fileDescriptor, &block, 16);

			if(block.inode == i_number_file) {
				deletion_j = j;
				deletion_addr_index = i;
			}

			if(block.inode==0) {
				end_addr_index = i;
				end_j = j;
				break;
			}
		}
		if(end_j!=BLOCK_SIZE/16) {
			break;
		}
	}
	if(end_j==0) {
		if(end_addr_index==0) {
			printf("File not present in directory!\n\n");
			return;
		} else {
			lseek(fileDescriptor, (temp_inode.addr[end_addr_index-1])*BLOCK_SIZE + ((BLOCK_SIZE/16)-1)*16, SEEK_SET);
			read(fileDescriptor, &block, 16);
			lseek(fileDescriptor, (temp_inode.addr[deletion_addr_index])*BLOCK_SIZE + (deletion_j)*16, SEEK_SET);
			write(fileDescriptor, &block, 16);
			lseek(fileDescriptor, (temp_inode.addr[end_addr_index-1])*BLOCK_SIZE + ((BLOCK_SIZE/16)-1)*16, SEEK_SET);
		}
	} else {
		lseek(fileDescriptor, (temp_inode.addr[end_addr_index])*BLOCK_SIZE + (end_j-1)*16, SEEK_SET);
		read(fileDescriptor, &block, 16);
		lseek(fileDescriptor, (temp_inode.addr[deletion_addr_index])*BLOCK_SIZE + (deletion_j)*16, SEEK_SET);
		write(fileDescriptor, &block, 16);
		lseek(fileDescriptor, (temp_inode.addr[end_addr_index])*BLOCK_SIZE + (end_j-1)*16, SEEK_SET);
	}

	block.inode = 0;
	block.filename[0] = '\0';
	write(fileDescriptor, &block, 16);
	add_inode_to_inode_list(i_number_file);
	printf("File has been deleted.\n\n");
}

int rm_dir() {
	short temp_currentInode = currentInode;
	char* folderName = strtok(NULL," ");
	folderName = parse_path(folderName);
	inode_type temp_inode;
	short i_number = get_inode_by_file_name(folderName);
	if(i_number==-1) {
		printf("Directory is not present in the filesystem!\n\n");
	} else {
		// Check if the file to be deleted is a directory or a file
		lseek(fileDescriptor, 2*BLOCK_SIZE + (i_number-1)*INODE_SIZE, SEEK_SET);
		read(fileDescriptor, &temp_inode, INODE_SIZE);
		if(!(temp_inode.flags & 0x4000)) {
			printf("The input given is a file!\n\n");
		} else {
			rm_folder_using_inumber(i_number);
		}
	}
	update_current_inode(temp_currentInode);
	return 0;
}

void rm_folder_using_inumber(short i_number) {
	int i, j;
	dir_type block;

	//Used for setting the block to 0. Int = 4 bytes thus we require a buffer of size 1024/4
	unsigned int buffer[BLOCK_SIZE/4];

	for(i=0; i<BLOCK_SIZE/4; i++) {
		buffer[i] = 0;
	}
	inode_type temp_inode;
	// Deleting the file contents and adding the blocks to the free list
	lseek(fileDescriptor, 2*BLOCK_SIZE + (i_number-1)*INODE_SIZE , SEEK_SET);
	read(fileDescriptor, &temp_inode, INODE_SIZE);
	for(i=0; i<ADDR_SIZE; i++) {
		for(j=(i==0)?2:0; j<(BLOCK_SIZE/16); j++) {
			lseek(fileDescriptor, BLOCK_SIZE*(temp_inode.addr[i]) + j*16, SEEK_SET);
			read(fileDescriptor, &block, 16);
			
			// If we hit the end in one of the ADDR blocks, we don't need to traverse in the rest of them. So break.
			if(block.inode != 0) {
				printf("Cannot delete the directory as there is data present inside!\n");
				return;
			}
		}
	}

	// Deleting entry from the directory
	lseek(fileDescriptor, BLOCK_SIZE*(temp_inode.addr[0]) + 16, SEEK_SET);
	read(fileDescriptor, &block, 16);

	lseek(fileDescriptor, BLOCK_SIZE*(temp_inode.addr[0]), SEEK_SET);
	write(fileDescriptor, &buffer, BLOCK_SIZE);
	//printf("Block %d added to the free list.\n", temp_inode.addr[0]);
	add_block_to_free_list(temp_inode.addr[0], buffer);

	lseek(fileDescriptor, 2*BLOCK_SIZE + (block.inode-1)*INODE_SIZE, SEEK_SET);
	read(fileDescriptor, &temp_inode, INODE_SIZE);
	int end_j = BLOCK_SIZE/16;
	int end_addr_index;
	int deletion_j;
	int deletion_addr_index;
	for(i=0; i<ADDR_SIZE; i++) {
		for(j=0; j<(BLOCK_SIZE/16); j++) {
			lseek(fileDescriptor, (temp_inode.addr[i])*BLOCK_SIZE + j*16, SEEK_SET);
			read(fileDescriptor, &block, 16);

			if(block.inode == i_number) {
				deletion_j = j;
				deletion_addr_index = i;
			}

			if(block.inode==0) {
				end_addr_index = i;
				end_j = j;
				break;
			}
		}
		if(end_j!=BLOCK_SIZE/16) {
			break;
		}
	}
	if(end_j==0) {
		if(end_addr_index==0) {
			printf("Folder not present in directory!\n\n");
			return;
		} else {
			lseek(fileDescriptor, (temp_inode.addr[end_addr_index-1])*BLOCK_SIZE + ((BLOCK_SIZE/16)-1)*16, SEEK_SET);
			read(fileDescriptor, &block, 16);
			lseek(fileDescriptor, (temp_inode.addr[deletion_addr_index])*BLOCK_SIZE + (deletion_j)*16, SEEK_SET);
			write(fileDescriptor, &block, 16);
			lseek(fileDescriptor, (temp_inode.addr[end_addr_index-1])*BLOCK_SIZE + ((BLOCK_SIZE/16)-1)*16, SEEK_SET);
		}
	} else {
		lseek(fileDescriptor, (temp_inode.addr[end_addr_index])*BLOCK_SIZE + (end_j-1)*16, SEEK_SET);
		read(fileDescriptor, &block, 16);
		lseek(fileDescriptor, (temp_inode.addr[deletion_addr_index])*BLOCK_SIZE + (deletion_j)*16, SEEK_SET);
		write(fileDescriptor, &block, 16);
		lseek(fileDescriptor, (temp_inode.addr[end_addr_index])*BLOCK_SIZE + (end_j-1)*16, SEEK_SET);
	}

	block.inode = 0;
	block.filename[0] = '\0';
	write(fileDescriptor, &block, 16);
	add_inode_to_inode_list(i_number);
	printf("Folder has been deleted\n\n");
}

int ls() {
	printf("The files present in the directory are:\n");
	int i,j;
	for(i=0; i<ADDR_SIZE; i++) {
		for(j=(i==0)?2:0; j<(BLOCK_SIZE/16); j++) {
			dir_type block;
			lseek(fileDescriptor, BLOCK_SIZE*(inode.addr[i]) +j*16, SEEK_SET);
			read(fileDescriptor, &block, 16);
			
			// If we hit the end in one of the ADDR blocks, we don't need to traverse in the rest of them. So break.
			if(block.inode == 0) {
				printf("\n");
				return;
			}
			printf("%s\n", block.filename);
		}
	}
	return 0;
}

int change_dir(char* pathname){
	char* token[INPUT_SIZE];
	if(pathname[0] == '/'){    
        currentInode = 1;                
    } 
	short cur_node,i=0;
    token[i] = strtok(pathname, "/");
	if(token[i] == NULL){
		currentInode = 1;
		lseek(fileDescriptor,2*BLOCK_SIZE + (currentInode-1)*INODE_SIZE,0);
		read(fileDescriptor,&inode,INODE_SIZE);
	}
    while (token[i] != NULL) {      
		cur_node = get_inode_by_file_name(token[i]);
		if(cur_node == -1) {
			printf("Invalid path given!\n\n");
			return 1;
		}
		else if(check_if_dir(cur_node)){
			currentInode = cur_node;
			lseek(fileDescriptor,2*BLOCK_SIZE + (currentInode-1)*INODE_SIZE,0);
			read(fileDescriptor,&inode,INODE_SIZE);
		}
		else{
			printf("Invalid path given!\n\n");
			return 1;
		}
		i++;
        token[i] = strtok(NULL, "/");
		
    }
	printf("Directory changed\n\n");
    return 0;
}

int check_if_dir(short cur_node){
	inode_type temp_inode;
	lseek(fileDescriptor,2*BLOCK_SIZE + (cur_node-1)*INODE_SIZE,0);
	read(fileDescriptor,&temp_inode,INODE_SIZE);
	if((temp_inode.flags & dir_flag) == dir_flag){
		return 1;
	}
	else{
		return 0;
	}
}

char* concat(const char *s1, const char *s2){
    const size_t len1 = strlen(s1);
    const size_t len2 = strlen(s2);
    char *result = malloc(len1 + len2 + 1); // +1 for the null-terminator
    memcpy(result, s1, len1);
    memcpy(result + len1, s2, len2 + 1); // +1 to copy the null-terminator
    return result;
}

int present_working_directory(){
	char* pwd;
	pwd = "/";
	pwd = pwd_recurse(currentInode,pwd);
	printf("%s\n\n",pwd);
	return 0;
}

char* pwd_recurse(short i_no,char* result){
	if(i_no == 1){
		return result;
	}
	char* dir_name;
	dir_type block;
	inode_type temp_inode;
	inode_type parent_inode;
	int i, j;
	
	lseek(fileDescriptor,BLOCK_SIZE*2+INODE_SIZE*(i_no - 1),0);
	read(fileDescriptor,&temp_inode,INODE_SIZE);
	
	lseek(fileDescriptor,BLOCK_SIZE*temp_inode.addr[0]+16,SEEK_SET);
	read(fileDescriptor,&block,16);
	short parent_inode_no = block.inode;
	lseek(fileDescriptor,BLOCK_SIZE*2+INODE_SIZE*(parent_inode_no - 1),0);
	read(fileDescriptor,&temp_inode,INODE_SIZE);
	for(i=0; i<ADDR_SIZE; i++) {
		if(inode.addr[i]!=0){
			for(j=0; j<(BLOCK_SIZE/16); j++) {
				lseek(fileDescriptor,BLOCK_SIZE*temp_inode.addr[i]+16*j,SEEK_SET);
				read(fileDescriptor,&block,16);
				if(block.inode == i_no) {
					dir_name = block.filename;
					//memcpy(dir_name,block.filename,strlen(block.filename));
					goto end_loop;
				}
			}
		}
	}
	end_loop:
		dir_name = concat("/",dir_name);
		//printf("%s\n",dir_name);
		result = concat(dir_name,result);
	return pwd_recurse(parent_inode_no,result);
}