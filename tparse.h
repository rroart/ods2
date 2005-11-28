#define	 TPA_EXIT    (void *)1
#define	 TPA_FAIL    (void *)0

#define	 TPA_ANY     (void *)254
#define	 TPA_ALPHA   (void *)253
#define	 TPA_DIGIT   (void *)252
#define	 TPA_STRING  (void *)251
#define	 TPA_SYMBOL  (void *)250
#define	 TPA_HEX     (void *)249
#define	 TPA_OCTAL   (void *)248
#define	 TPA_DECIMAL (void *)247
#define	 TPA_LAMBDA  (void *)246
#define	 TPA_EOS     (void *)245
#define	 TPA_SUB(a)  ((void *)(void *[]){(void *)244, (void *)a})
#define	 TPA_END  {NULL, NULL, NULL, 0, NULL, 0}

/*
  The following definition must match the size of void *
*/

#define	 lu	     (long unsigned)

/*
  The string *str is pointing to must be writable
  or tparse will fail.
*/

typedef struct argblk {
  long unsigned        options;
  char                *str;
  char                *token;
  long unsigned        number;
  long unsigned        param;
  long unsigned        arg;
  long unsigned        mask;
  long unsigned       *mskadr;
} ARGBLK;


typedef struct tparse {
  char                *type;
  struct tparse       *label;
  int                 (*action)(ARGBLK *);
  unsigned long        mask;
  unsigned long       *mskadr;
  unsigned long        param;
} TPARSE;

int tparse(ARGBLK *argblk, TPARSE *tpa);
