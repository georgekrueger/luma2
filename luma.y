
%{
#include <math.h>
int yylex (void);
void yyerror (char const *);

typedef double (*func_t) (double);

///////////////////////////
// Scales
///////////////////////////
typedef enum
{
	CMAJ,
	CMIN,
	NO_SCALE,
} Scale;

const int NumScales = NO_SCALE;

char const * const ScaleNames[NumScales] = 
{
	"cmaj",
	"cmin",
};

const char* GetScaleName(Scale scale)
{
	return ScaleNames[scale];
}

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


#include <vector>
#include <iostream>
using namespace std;

class Note
{
public:
	Note(Scale scale, int octave, int degree, float velocity, int length) :
		scale_(scale), octave_(octave), degree_(degree), velocity_(velocity), 
		length_(length), isRest_(false)
	{
	}

	Note(int length) : length_(length), isRest_(true) {}
	~Note() {}

	short GetLength() { return length_; }
	//short GetPitch() { return pitch_; }
	short GetVelocity() { return velocity_; }
	
	void SetRest(bool b) { isRest_ = b; }

	void Print()
	{
		if (isRest_) {
			cout << "rest " << length_;
		}
		else {
			cout << GetScaleName(scale_) << " " << octave_ << " " << degree_
				 << " " << velocity_ << " " << length_;
		}
		cout << " ";
	}

private:
	Scale scale_;
	int octave_;
	int degree_;
	float velocity_;
	int length_;
	bool isRest_;
};

class Pattern
{
public:
	Pattern() {}
	~Pattern() {}

private:
	/*struct PNote
	{
		PNote(Note* n, unsigned long off) : note(*n), offset(off) {}
		Note note;
		unsigned long offset;
	};*/

public:
	void Add(Note* n)
	{
		notes_.push_back(*n);
	}

	void Add(Pattern* p)
	{
		for (unsigned long i=0; i<p->GetNumNotes(); i++) {
			Note* n = p->GetNote(i);
			Add(n);
		}
	}

	Note* GetNote(unsigned long i) {
		return &notes_[i];
	}

	size_t GetNumNotes() { 
		return notes_.size(); 
	}

	void Print()
	{
		cout << endl << "Pattern size: " << notes_.size() << endl;
		for (unsigned long i=0; i<notes_.size(); i++) {
			notes_[i].Print();
		}
		cout << endl;
	}

private:
	vector<Note> notes_;
	//unsigned long writeOffset_;
};

Pattern masterPattern;

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

noteexp:  SCALE'_'NUM'_'NUM'_'NUM'_'NUM
{
	Scale scale = $1->value.scale;
	int octave = $3;
	int degree = $5;
	int velocity = $7;
	int length = $9;
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

pattern:  '['patseq']'
{
	$$ = $2;
	$2->Print();
};

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

int yylex (void)
 {
   int c;

   /* Ignore white space, get first nonwhite character.  */
   while ((c = getchar ()) == ' ' || c == '\t');

   if (c == EOF)
	 return 0;

   /* Char starts a number => parse the number.         */
   if (c == '.' || isdigit (c))
	 {
	   ungetc (c, stdin);
	   scanf ("%d", &yylval.val);
	   return NUM;
	 }

	/*if (c == '_') {
		int restLength;
		scanf ("%d", &restLength);
		int lengthMod;
		lengthMod = getchar();
		if (lengthMod == 'B')
			restLength *= 4; 
		yylval.val = restLength;
		cout << "rest" << endl;
		return REST;
	}*/

   /* Char starts an identifier => read the name.       */
   if (isalpha (c))
	 {
	   symrec *s;
	   static char *symbuf = 0;
	   static int length = 0;
	   int i;

	   /* Initially make the buffer long enough
		  for a 40-character symbol name.  */
	   if (length == 0)
		 length = 40, symbuf = (char *)malloc (length + 1);

	   i = 0;
	   do
		 {
		   /* If buffer is full, make it bigger.        */
		   if (i == length)
			 {
			   length *= 2;
			   symbuf = (char *) realloc (symbuf, length + 1);
			 }
		   /* Add this character to the buffer.         */
		   symbuf[i++] = c;
		   /* Get another character.                    */
		   c = getchar ();
		 }
	   while (isalnum (c));

	   ungetc (c, stdin);
	   symbuf[i] = '\0';

	   s = getsym (symbuf);
	   if (s == 0) {
		 s = putsym (symbuf, VAR);
         printf("Adding new symbol: %s\n", s->name);
	   }
	   yylval.tptr = s;
	   //cout << "Tokenized symbol: " << s->name << endl;
	   //print_sym_type(s);
	   return s->type;
	 }

   /* Any other character is a token by itself.        */
   return c;
 }

struct init
{
	char const *fname;
	double (*fnct) (double);
};

 struct init const arith_fncts[] =
 {
   "sin",  sin,
   "cos",  cos,
   "atan", atan,
   "ln",   log,
   "exp",  exp,
   "sqrt", sqrt,
   0, 0
 };

/* The symbol table: a chain of `struct symrec'.  */
symrec *sym_table;

 /* Put arithmetic functions in table.  */
void init_table (void)
{
	int i;
	symrec *ptr;
	for (i = 0; arith_fncts[i].fname != 0; i++)
	{
		ptr = putsym (arith_fncts[i].fname, FNCT);
		ptr->value.fnctptr = arith_fncts[i].fnct;
	}
	for (i = 0; i < NumScales; i++)
	{
		ptr = putsym (ScaleNames[i], SCALE);
		ptr->value.scale = (Scale)i;
	}
}

int main (void)
{
	init_table();
	return yyparse ();
}

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

