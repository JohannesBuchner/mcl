/*          Copyright (C) 2002, EMBL-EBI, author Anton Enright.
 *
 * This file is part of MCL.  You can redistribute and/or modify MCL under the
 * terms of the GNU General Public License; either version 2 of the License or
 * (at your option) any later version.  You should have received a copy of the
 * GPL along with MCL, in the file COPYING.
*/

/* ================================================================== */
/* markov.c:                                                          */
/* Generates an MCL standard matrix from parsed BLAST similarity Data */
/* Uses simple symmetrification to ensure all hits are symmetric      */
/*                                                                    */ 
/* 		           VERSION 1.0a                                      */
/*                                                                    */
/* Anton Enright  - Computational Genomics Group (C. Ouzounis)        */
/*                                                                    */
/* EMBL - European Bioinformatics Institute                           */
/* ================================================================== */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "tribe-matrix.h"

void dump_hits (int, struct hit_struct *);
int sort_alpha_routine (const void *s1,const void *s2);
int sort_alpha2 (const void *s1,const void *s2);
int  binary_search_routine (const void *s1, const void *s2);

int main (int argc, char *argv[])
{
struct hit_struct *hits;
FILE *fp;
FILE *fpout;
FILE *indexout;
char protein1[100];
char protein2[100];
char file1[100]="proteins.index";
char file2[100]="matrix.mci";
int evalue1;
int evalue2;
double power;
char *protein_index[2000000];
int total_proteins=0;

char BUFFER[400];
int total_hits=0;
int new_hits=0;
int index=0;
int index1=0;
int index2=0;
int allocatedmem=0;
int allocatedchunk=0;
int chunksize=20;
int i=0;
int j=0;
char *key,**key1,*fptr;
struct hit_struct *ptr;
struct hit_struct searchkey;
struct hit_struct *searchptr;

if (argc == 1)
	{
	printf("\nMarkov Matrix Generation Tool\n");
	printf("-----------------------------\n");
	printf("Usage:\n");
	printf("\nmarkov parsedfile\n");
	printf("\nOr markov -h for help\n\n");
	exit(1);
	}

for (i=0;i < argc;i++)
        {
	if ( (!strcmp(argv[i],"-h")) || (!strcmp(argv[i],"-help")) )	
		{
		printf("\nMarkov Matrix Generation Tool\n");
		printf("-----------------------------\n");
		printf("Usage:\n");
		printf("\nmarkov parsedfile\n");
		printf("\nOr markov -h for help\n\n");

		printf("-----------------------------\n");
		printf("Options:\n");
		printf("\n-ind somefile		Output Index to file \'somefile\' (Default %s)\n",file1);
		printf("-out somefile		Output MCL-Matrix to file \'somefile\' (Default %s)\n",file2);
		printf("-chunk X		Use X (Megabyte) Chunks when allocating memory (Default %d MB)\n",chunksize);
		printf("\n\n");

		exit(1);
		}

	if ( !strcmp(argv[i],"-ind") && (i+1 < argc) )
		{
		strcpy(file1,argv[i+1]);
		}

	if ( !strcmp(argv[i],"-out") && (i+1 < argc) )
                {
		strcpy(file2,argv[i+1]);
                }

	if ( !strcmp(argv[i],"-chunk") && (i+1 < argc) )
                {
                printf("ChunkSize %s\n",argv[i+1]);
		j=atoi(argv[i+1]);
		if ((j > 0) && (j <= 600))
			{
			chunksize=j;
			}
		else
			{
			fprintf(stderr,"Error: Chunksize Invalid or >600MB\n");
			exit(1);
			}
                }
	}

printf("\nMarkovMatrix v1.0 (c) EMBL-EBI\n");
printf("------------------------------\n");
printf("anton@ebi.ac.uk (2001)\n");
printf("\n\n");
allocatedchunk=(int) ceil((1048576*chunksize)/sizeof(struct hit_struct));
printf("Initial Chunksize is %d (Similarities) using %ld MB\n",allocatedchunk,
(long)(allocatedchunk*(sizeof(struct hit_struct))/1048576));

if ((hits=(struct hit_struct *) malloc(allocatedchunk*sizeof(searchkey))))
	{
	printf("Allocated Initial %f MB\n",floor(allocatedchunk*(sizeof(struct hit_struct))/1048576));
	allocatedmem=allocatedchunk;
	}

printf("-------------------------------------------------------------\n");
printf("Beginning Markov Matrix Procedure:\n");
printf("-------------------------------------------------------------\n");
protein_index[total_proteins]=(char *)malloc(100*sizeof(char));

if ((fp=fopen(argv[1],"r")) == NULL)
	{
	fprintf(stderr,"File cannot be opened\n");
	exit(1);
	}

printf("\n\nReading In Hits:\n");
printf("--------------------\n");
while(!feof(fp))
        {
	memset(BUFFER, '\0', 400 );
	memset(protein1,'\0',100);
	memset(protein2,'\0',100);
        if (!feof(fp))
	        {
		memset(BUFFER, '\0', 400 );
        	memset(protein1,'\0',100);
        	memset(protein2,'\0',100);
        	fgets(BUFFER, 400, fp);
        	sscanf(BUFFER,"%s\t%s\t%d\t%d\n",protein1,protein2,&evalue1,&evalue2); 
	if ((total_hits > 0) && (!strcmp(hits[total_hits-1].protein1,protein1)) && (!strcmp(hits[total_hits-1].protein2,protein2)) )
			{
			}
		else
			{

			if ((protein1[0]!='\0') && (protein2[0]!='\0') && (strcmp(protein1,"PARSED>")))
			{
			strcpy(hits[total_hits].protein1,protein1);
			strcpy(hits[total_hits].protein2,protein2);
			hits[total_hits].evalue1=evalue1;
			hits[total_hits].evalue2=evalue2;
			total_hits++;
			if (total_hits == allocatedmem)
				{
				printf("Realloc:\t");
				fflush(stdout);
				allocatedmem+=allocatedchunk;
				if ((hits=(struct hit_struct *) realloc(hits,allocatedmem*sizeof(searchkey))))
					{
        				printf("Allocated %f MB\n",floor(allocatedchunk*(sizeof(struct hit_struct))/1048576));
					
					}

				}
			}
			}
		}
	}

printf("Read %d Hits\n",total_hits);
printf("--------------------\n");
fflush(stdout);
qsort (hits,total_hits,sizeof(struct hit_struct),sort_alpha_routine);
printf("--------------------\n");
fflush(stdout);

printf("\n\nSymmetrification in Progress\n");

for (i=0;i<total_hits;i++)
{
strcpy(searchkey.protein1,hits[i].protein2);
strcpy(searchkey.protein2,hits[i].protein1);
searchptr=&searchkey;

ptr=(struct hit_struct *)bsearch(searchptr,hits,total_hits,sizeof(hits[0]),binary_search_routine);
if (ptr==NULL)
	{
	if (total_hits == allocatedmem)
             {
             printf("Realloc:\t");
             fflush(stdout);
             allocatedmem+=allocatedchunk;
             if ((hits=(struct hit_struct *) realloc(hits,allocatedmem*sizeof(searchkey))))
                 {
                 printf("Allocated %f MB\n",floor(allocatedchunk*(sizeof(struct hit_struct))/1048576));
                 }
             }

	strcpy(hits[total_hits].protein2,hits[i].protein1);
	strcpy(hits[total_hits].protein1,hits[i].protein2);
	hits[total_hits].evalue1=hits[i].evalue1;
	hits[total_hits].evalue2=hits[i].evalue2;
	/*printf("Non Symmetric Hit %s %s\n",hits[i].protein1,hits[i].protein2); */
	total_hits++;
	new_hits++;
	}
}

printf("Corrected %d Hits, Total Hits %d\n",new_hits,total_hits);
printf("--------------------\n");
fflush(stdout);
qsort (hits,total_hits,sizeof(struct hit_struct),sort_alpha_routine);

printf("\nCreating Index File %s\n",file1); 
fflush(stdout);

if ((indexout=fopen(file1,"w")) == NULL)
	{
	printf("Error cannot create index file %s\n",file1);
	exit(1);
	}

for (i=0;i<total_hits;i++)
	{
	if (total_hits > 0)
		{
		if (strcmp(hits[i].protein1,hits[i-1].protein1))
			{
			protein_index[total_proteins]=(char *)malloc(100*sizeof(char));
			strcpy(protein_index[total_proteins],hits[i].protein1);
			fprintf(indexout,"%d\t%s\n",total_proteins,hits[i].protein1);
			/*printf("%d\t%s\n",total_proteins,hits[i].protein1);*/
			total_proteins++;
			}
		}
	}
printf("Total Proteins: %d\n",total_proteins);

/*
for (i=0;i<total_proteins;i++)
	{
	printf("%d\t%s\n",i,protein_index[i]);
	fflush(stdout);
	}
*/

printf("\nCreating Markov Matrix file %s\n",file2);
fflush(stdout);
if ((fpout=fopen(file2,"w")) == NULL)
        {
        fprintf(stderr,"File %s cannot be created\n",file2);
        exit(1);
        } 

fprintf(fpout,"(mclheader\nmcltype matrix\ndimensions %dx%d\n)\n(mclmatrix\nbegin\n",total_proteins,total_proteins);
fprintf(fpout,"0");
for (i=0;i<total_hits;i++)
        {
	strcpy(protein1,hits[i].protein1);
	strcpy(protein2,hits[i].protein2);
	if (hits[i].evalue1==0 || hits[i].evalue2==0)
		{
		hits[i].evalue1=1;
		hits[i].evalue2=200;
		}
	power=(double) hits[i].evalue1*(double) (pow(10,(double) -((double)hits[i].evalue2)));
	power=(double) -log10(power);
	
	
	key=protein1;
	key1=&key;

	fptr=(char *)bsearch(key1,protein_index,total_proteins,sizeof(char *),sort_alpha2);

	if (fptr == NULL)
		{
		printf("Error protein not found (%d) %s\n",total_hits,protein1);
		}	
	else
		{
		index1=((fptr - (char *)protein_index)/sizeof(char *));
		}

	key=protein2;
        key1=&key;
 
        fptr=(char *)bsearch(key1,protein_index,total_proteins,sizeof(char *),sort_alpha2);
 
        if (fptr == NULL)
                {
                printf("Error protein not found (%d) %s\n",total_hits,protein2);
                }
        else
                {
                index2=((fptr - (char *)protein_index)/sizeof(char *));
                }
	
	if (index1 != index)
		{
		fprintf(fpout," $\n%d",index1);
		}

	fprintf(fpout," %d:%d",index2,(int) rint(power));

	index=index1;
        } 
fprintf(fpout," $\n)\n");

printf("\n\nComplete!\n");
return(0);
}

int sort_alpha_routine (const void *s1,const void *s2)
	{

	struct hit_struct *a;
	struct hit_struct *b;

	a=(struct hit_struct *) s1;
	b=(struct hit_struct *) s2;

	
	if (strcmp(a->protein1,b->protein1) == 0)
		{
		return(strcmp(a->protein2,b->protein2));
		}
	else
		{
		return(strcmp(a->protein1,b->protein1));
		}
	}

int sort_alpha2(const void *s1,const  void *s2)
        {
	fflush(stdout);
	return(strcmp(*(char **)s1, *(char **)s2));
        }

int binary_search_routine(const void *s1,const void *s2)
        {
        struct hit_struct *a;
        struct hit_struct *b;
 
        a=(struct hit_struct *) s1;
        b=(struct hit_struct *) s2;


	if (strcmp(a->protein1,b->protein1)==0)
		{
		return(strcmp(a->protein2,b->protein2));
		}
	else
		{
		return(strcmp(a->protein1,b->protein1));
		}
        }

void dump_hits(int total_hits,struct hit_struct *hits)
	{
	int i=0;
	printf("Total Hits %d\n",total_hits);
	for (i=0;i<total_hits;i++)
		{
		printf("%d> %s %s\n",i,hits[i].protein1,hits[i].protein2);	
		}
	}
