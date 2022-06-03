// Matus Fabo xfabom01 Vladimir Drengubiak xdreng01
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>

#define CHUNK_SIZE 128

typedef struct thread_args{
	int thread_id;
	char* substring;
	int str_value;
} t_args;

pthread_mutex_t** mutex;
pthread_mutex_t thread_kill;
pthread_mutex_t thread_flag_lock;
int threads_done;	// Critical varaible
					// "Flag" to check if all threads are done ('threads_done' == 'thread_count')
char* line;
int line_score;		// Critical variable


void mutex_thread_kill(pthread_t* threads, int size){
	pthread_mutex_unlock(&thread_kill);
	for(int i = 0; i < size; i++){
		pthread_join(threads[i], NULL);
		free(mutex[i]);
	}
	free(threads);
	free(mutex);
}

int read_line(){
	/// Check 'line'
	if(line != NULL) free(line);

	/// Allocate 'line' buffer
	int line_size = CHUNK_SIZE;
	line = malloc(sizeof(char)*CHUNK_SIZE);
	if(line == NULL){
		fprintf(stderr, "Line allocation failed.\n");
		return 0;
	}

	/// Load one line char by char from 'stdin'
	int line_index = 0;
	int c = fgetc(stdin);
	if(c == EOF) return 0;
	while(c != '\n' && c != EOF){
		line[line_index++] = c;
		if(line_index >= line_size){
			/// Thats a big line u got there, lemme resize the buffer :*
			line_size += CHUNK_SIZE;
			char* tmp = realloc(line, line_size);
			if(tmp == NULL){
				fprintf(stderr, "Line allocation failed.\n");
				free(line);
				return 0;
			}
			else line = tmp;
		}
		c = fgetc(stdin);
	}
	line[line_index] = '\0';

	return 1;
}

void* thread_search(void* thread_arguments){
	/// Convert given arguments
	const int	id   = ((t_args*)thread_arguments)->thread_id;
	const char*	subs = ((t_args*)thread_arguments)->substring;
	const int	val  = ((t_args*)thread_arguments)->str_value;
	int found = 0; // Either 0 or 'val', depends if line contains the substring
	int line_index = 0;
	int c;
	char state = 1;

	/// Helper mutex lock for natural thread termination
	while(pthread_mutex_trylock(&thread_kill)){
		/// Wait for the line
		if(!pthread_mutex_trylock(mutex[id])){
			/// Loop through the line and look for 'subs' match
			c = line[line_index++];
			while(c != '\0' && state != -1){
				/// Finite state machine to find the substring
				switch(state){
					case 1:	if(c == subs[0]) state = 2;
							break;
					case 2:	if(c == subs[1]) state = 3;
							else if(c == subs[0]) state = 2;
							else state = 1;
							break;
					case 3:	if(c == subs[2]) state = 4;
							else if(c == subs[0]) state = 2;
							else state = 1;
							break;
					case 4:	if(c == subs[3]) state = 5;
							else if(c == subs[0]) state = 2;
							else state = 1;
							break;
					case 5:	if(c == subs[4]){
								found = val;
								state = -1; // Unused state to end the loop, the number is "random" 
							}
							else if(c == subs[0]) state = 2;
							else state = 1;
							break;
				}

				c = line[line_index++];
			}

			/// Update line score and signal that the thread is done
			pthread_mutex_lock(&thread_flag_lock);
			line_score += found;
			threads_done++;
			pthread_mutex_unlock(&thread_flag_lock);

			/// Reset appropriate varibles
			state = 1;
			found = 0;
			line_index = 0;
		}
	}
	pthread_mutex_unlock(&thread_kill);
	return NULL;
}

int main(int argc, char* argv[]){
	int thread_count = 0;
	int print_threshold;
	t_args* thread_args_arr; // Thread argumet wrapper array
	pthread_t* threads;
	threads_done = 0;
	line_score = 0;
	line = NULL;

	/// Check function arguments
	if(!(argc%2) && argc >= 4){
		/// Establish 'thread_count', 'print_threshold' and thread arguments
		thread_count = (argc-2)/2;
		print_threshold = atoi(argv[1]);
		thread_args_arr = malloc(sizeof(t_args)*thread_count);
		if(thread_args_arr == NULL){
			fprintf(stderr, "Allocation failed.\n");
			return 1;
		}
		for(int i = 0; i < thread_count; i++){
			if(strlen(argv[2+(i*2)]) != 5){
				fprintf(stderr, "Delka STR%d != 5\n", i+1);
				free(thread_args_arr);
				return 1;
			}
			thread_args_arr[i].thread_id = i;
			thread_args_arr[i].substring = argv[2+(i*2)];
			thread_args_arr[i].str_value = atoi(argv[3+(i*2)]);
		}
	}
	else{
		fprintf(stdout, "USAGE: aut MIN_SCORE STR1 SC1 [ STR2 SC2 ] [ STR3 SC3 ] ...\n");
		return 1;
	}

	/// Initialize static mutex locks
	if(pthread_mutex_init(&thread_kill, NULL) ||
		pthread_mutex_init(&thread_flag_lock, NULL)){
		fprintf(stderr, "Lock initialization failed.\n");
		free(thread_args_arr);
	}
	pthread_mutex_lock(&thread_kill);

	/// Allocate enough space for dynamic mutex locks
	mutex = malloc(sizeof(pthread_mutex_t*)*thread_count);
	if(mutex == NULL){
		fprintf(stderr, "Allocation failed.\n");
		free(thread_args_arr);
		return 1;
	}
	for(int i = 0; i < thread_count; i++) mutex[i] = NULL;

	/// Allocate space for threads
	threads = malloc(sizeof(pthread_t)*thread_count);
	if(threads == NULL){
		fprintf(stderr, "Allocation failed.\n");
		free(thread_args_arr);
		free(mutex);
		return 1;
	}

	/// Create each thread and allocate mutex for the thread
	for(int i = 0; i < thread_count; i++){
		mutex[i] = malloc(sizeof(pthread_mutex_t));
		if(mutex[i] == NULL || pthread_mutex_init(mutex[i], NULL)){
			fprintf(stderr, "Lock initialization failed.\n");
			mutex_thread_kill(threads, i);
			free(thread_args_arr);
			return 1;
		}
		pthread_mutex_lock(mutex[i]);
		if(pthread_create(threads+i, NULL, &thread_search, (void*)&thread_args_arr[i]) != 0){
			fprintf(stderr, "Thread creation failed.\n");
			free(thread_args_arr);
			mutex_thread_kill(threads, i);
			free(mutex[i]);
			return 1;
		}
	}

	/// Load each line
	while(read_line()){
		/// Start thread search
		for(int i = 0; i < thread_count; i++){
			pthread_mutex_unlock(mutex[i]);
		}

		/// Wait until all threads are done
		while(thread_count != threads_done);

		/// Print out the line if appropriate
		if(line_score >= print_threshold) fprintf(stdout, "%s\n", line);

		/// Reset variables
		threads_done = 0;
		line_score = 0;
	}

	/// Free all resources and destroy all threads
	mutex_thread_kill(threads, thread_count);
	free(thread_args_arr);
	if(line != NULL) free(line);
	return 0;
}