#define _GNU_SOURCE


#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sched.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <math.h>


#define N_CORES 4

struct CalcStr
{
	double st, fin, delta, cur, res;
	double* final_res;
	int sem_id, num_af;
	char trash[64];
};

struct Useless
{
	int num_af, sem_id;	
};


void* calculate(void* ptr)
{
	struct CalcStr* mem = (struct CalcStr*) ptr;

	cpu_set_t set;
	CPU_ZERO(&set);
	CPU_SET((mem->num_af), &set);
	sched_setaffinity(0, sizeof(set), &set);

	//affinity




	for (mem->cur = mem->st; mem->cur < mem->fin; mem->cur += mem->delta)
	{
		mem->res += (pow(mem->cur, 3) + pow(mem->cur + mem->delta, 3)) * mem->delta / 2;
	} 

	struct sembuf str;

	str.sem_num = 0;
	str.sem_op = -1;
	str.sem_flg = 0;

	semop(mem->sem_id, &str, 1);

	*mem->final_res += mem->res; 

	str.sem_num = 0;
	str.sem_op = 1;
	str.sem_flg = 0;

	semop(mem->sem_id, &str, 1);
	return NULL;
}


void* useless(void* smth)
{	
	struct Useless* arg = smth;

	cpu_set_t set;
	CPU_ZERO(&set);
	CPU_SET((arg->num_af), &set);
	sched_setaffinity(0, sizeof(set), &set);

//affinity

	struct sembuf buf[2];

	buf[0].sem_num = 1;
	buf[0].sem_op = -1;
	buf[0].sem_flg = IPC_NOWAIT;

	buf[1].sem_num = 1;
	buf[1].sem_op = 1;
	buf[1].sem_flg = 0;

	while(semop(arg->sem_id, buf, 2) == -1);

	return NULL;
}




/*x^3 [1, 3]*/
int main(int argc, char const *argv[])
{
	if (argc != 2)
	{
		printf("Wrong arguments\n");
		return 0;
	}

	int n = strtol(argv[1], NULL, 10);



	struct CalcStr* mem = (struct CalcStr*) calloc (2 * n, sizeof(struct CalcStr));


	int i = 0;

	int max = n > N_CORES ? n : N_CORES;

	pthread_t* tinfo = (pthread_t*) calloc (max, sizeof(pthread_t));


	double step = 2.0 / n;
	double start = 1.0;
	double res = 0;
	double delta = 2.0 / 10000000;


	int sem_id = semget(IPC_PRIVATE, 2, 0);

	struct sembuf str;

	str.sem_num = 0;
	str.sem_op = 1;
	str.sem_flg = 0;

	semop(sem_id, &str, 1);


	for (i = 0; i < n; ++i, start += step)
	{
		mem[2 * i].st = start;
		mem[2 * i].fin = start + step;
		mem[2 * i].num_af = i % N_CORES;
		mem[2 * i].sem_id = sem_id;
		mem[2 * i].final_res = &res;
		mem[2 * i].delta = delta;
		if (pthread_create(&tinfo[i], NULL, calculate,(void*) &(mem[i * 2])) == -1)
		{
			printf("Error create threads\n");
			exit(EXIT_FAILURE);
		}
	}

	struct Useless buf[N_CORES];

	if (n < N_CORES)
	{
		for (i = 0; i < N_CORES; ++i)
		{
			buf[i].sem_id = sem_id;
			buf[i].num_af = i;
		}

		for (i = n; i < N_CORES; ++i)
		{
			if (pthread_create(&tinfo[i], NULL, useless, (void*) &buf[i]) == -1)
			{
				printf("Error create threads\n");
				exit(EXIT_FAILURE);
			}
		}
	}

	for (i = 0; i < n; ++i)
	{
		if (pthread_join(tinfo[i], NULL) != 0)
		{
			printf("Error join thread: %d\n", i);
			exit(EXIT_FAILURE);
		}
	}

	str.sem_num = 1;
	str.sem_op = 1;
	str.sem_flg = 0;

	semop(sem_id, &str, 1);

	printf("%.3f\n", res);






	return 0;
}