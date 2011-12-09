
%{
#ifndef LUMA_GRAMMAR
#define LUMA_GRAMMAR

#include <math.h>
#include "music.h"

int yylex (void);
void yyerror (char const *);

typedef double (*func_t) (double);

 /* Data type for links in the chain of symbols.  */
struct symrec
{
	char *name;  /* name of symbol */
	int type;    /* type of symbol: either VAR or FNCT */
	union
	{
		double var;      /* value of a VAR */
		func_t fnctptr;  /* value of a FNCT */
		Scale scale;
	} value;
	struct symrec *next;  /* link field */
};

typedef struct symrec symrec;

/* The symbol table: a chain of `struct symrec'.  */
extern symrec *sym_table;

symrec *putsym (char const *, int);
symrec *getsym (char const *);

Song song;

%}

%union {
	int val; /* for numbers */
	symrec* tptr; /* for symbol table pointers */
	Note* note;
	Pattern* pat;
}
%token <val> NUM
%token <tptr> VAR FNCT SCALE
%type <note> noteexp;
%type <note> rest;
%type <pat> patseq;
%type <pat> pattern;

%expect 1

%% 

/* Grammar rules and actions follow.  */
input:    /* empty */
	 | input line
;

line:     '\n'
	 | pattern '\n'
;

rest:	'_' NUM 
{ 
	//cout << "rest  beats: " << $2 << endl; 
	Note* n = new Note($2);
	$$ = n; 
};

noteexp:  
	SCALE'_'NUM'_'NUM'_'NUM'_'NUM
	{
		Scale scale = $1->value.scale;
		int octave = $3;
		int degree = $5;
		int velocity = $7;
		int length = $9;
		//printf("Scale: %d Octave: %d Degree: %d\n", scale, octave, degree); 
		Note* n = new Note(scale, octave, degree, velocity, length);
		$$ = n;
	} |
	SCALE'_'NUM'_'NUM
	{
		Scale scale = $1->value.scale;
		int octave = $3;
		int degree = $5;
		int velocity = 100;
		int length = 4;
		//printf("Scale: %d Octave: %d Degree: %d\n", scale, octave, degree); 
		Note* n = new Note(scale, octave, degree, velocity, length);
		$$ = n;
	} |
	SCALE'_'NUM'_'NUM'_'NUM
	{
		Scale scale = $1->value.scale;
		int octave = $3;
		int degree = $5;
		int velocity = $7;
		int length = 4;
		//printf("Scale: %d Octave: %d Degree: %d\n", scale, octave, degree); 
		Note* n = new Note(scale, octave, degree, velocity, length);
		$$ = n;
	};

patseq:  
	noteexp 
	{
		Pattern* p = new Pattern;
		p->Add($1);
		$$ = p; 
	} | 
	pattern 
	{
		Pattern* p = new Pattern;
		p->Add($1);
		$$ = p; 
	} | 
	rest    
	{
		Pattern* p = new Pattern; 
		p->Add($1);
		$$ = p; 
	} |
	patseq','patseq 
	{
		Pattern* p = new Pattern;
		p->Add($1);
		p->Add($3);
		$$ = p;
	}
;

pattern:
	'[' patseq ']'
	{
		$$ = $2;
		$2->SetRepeatCount(1);
		song.AddPattern($2);
		$2->Print();
	} |
	'[' patseq ']' '#' NUM
	{
		$$ = $2;
		$2->SetRepeatCount($5);
		song.AddPattern($2);
		$2->Print();
	}
;

%%

#include <ctype.h>
#include <stdio.h>

void print_sym_type(symrec* sym)
{
	printf("SymTypeID: %d: ", sym->type);
	if (sym->type == FNCT) {
		printf("Function");
	}
	else if (sym->type == SCALE) {
		printf("Scale");
	}
	printf("\n");
}

ifstream is;

int yylex (void)
{
	int c;

	do {
		c = is.get();
	} while (c == ' ' || c == '\t');

	if (!is.good()) {
		return 0;
	}

	// Char starts a number => parse the number. 
	if (c == '.' || isdigit (c))
	{
		is.unget();
		is >> c;
		yylval.val = c;
		//cout << "Token Num: " << c << endl;
		return NUM;
	}

	// Char starts an identifier => read the name.
	if (isalpha (c))
	{
		symrec *s;
		static char *symbuf = 0;
		static int length = 0;
		int i;

		// Initially make the buffer long enough for a 40-character symbol name.
		if (length == 0) {
			length = 40;
			symbuf = (char *)malloc (length + 1);
		}

		i = 0;
		do
		{
			// If buffer is full, make it bigger.
			if (i == length)
			{
				length *= 2;
				symbuf = (char *) realloc (symbuf, length + 1);
			}
			// Add this character to the buffer
			symbuf[i++] = c;
			// Get another character
			c = is.get();
		}
		while (isalnum (c));

		is.unget();
		symbuf[i] = '\0';

		s = getsym (symbuf);
		if (s == 0) {
			s = putsym (symbuf, VAR);
			//cout << "Adding new symbol: " << s << endl;
		}
		yylval.tptr = s;
		//cout << "Token symbol: " << s->name << endl;
		//print_sym_type(s);
		return s->type;
	}

	//cout << "Token: " << (char)c << endl;

	// Any other character is a token by itself
	return c;
 }

/* The symbol table: a chain of `struct symrec'.  */
symrec *sym_table;

 /* Put arithmetic functions in table.  */
void init_table (void)
{
	int i;
	symrec *ptr;
	for (i = 0; i < NumScales; i++)
	{
		ptr = putsym (ScaleNames[i], SCALE);
		ptr->value.scale = (Scale)i;
	}
}

/*int main (int argc, char** argv)
{
	init_table();

	is.open("C:\\Documents and Settings\\George\\My Documents\\luma2\\input.txt", ifis::in);
	int ret = yyparse();
	is.close();

	return ret;
}*/

 /* Called by yyparse on error.  */
 void yyerror (char const *s)
 {
   fprintf (stderr, "%s\n", s);
 }

symrec * putsym (char const *sym_name, int sym_type)
 {
   symrec *ptr;
   ptr = (symrec *) malloc (sizeof (symrec));
   ptr->name = (char *) malloc (strlen (sym_name) + 1);
   strcpy (ptr->name,sym_name);
   ptr->type = sym_type;
   ptr->value.var = 0; /* Set value to 0 even if fctn.  */
   ptr->next = (struct symrec *)sym_table;
   sym_table = ptr;
   return ptr;
 }

 symrec * getsym (char const *sym_name)
 {
   symrec *ptr;
   for (ptr = sym_table; ptr != (symrec *) 0;
		ptr = (symrec *)ptr->next)
	 if (strcmp (ptr->name,sym_name) == 0)
	   return ptr;
   return 0;
 }

#endif