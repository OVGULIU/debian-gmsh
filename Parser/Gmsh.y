%{
// Gmsh - Copyright (C) 1997-2008 C. Geuzaine, J.-F. Remacle
//
// See the LICENSE.txt file for license information. Please report all
// bugs and problems to <gmsh@geuz.org>.

#include <string.h>
#include <stdarg.h>
#include <time.h>
#include "Message.h"
#include "MallocUtils.h"
#include "ListUtils.h"
#include "TreeUtils.h"
#include "Numeric.h"
#include "Context.h"
#include "GModel.h"
#include "Geo.h"
#include "GeoInterpolation.h"
#include "Generator.h"
#include "Draw.h"
#include "Options.h"
#include "Colors.h"
#include "Parser.h"
#include "OpenFile.h"
#include "CommandLine.h"
#include "FunctionManager.h"
#include "ColorTable.h"
#include "OS.h"
#include "CreateFile.h"
#include "gmshSurface.h"
#include "Field.h"
#include "BackgroundMesh.h"

#if !defined(HAVE_NO_POST)
#include "PView.h"
#include "PViewDataList.h"
#include "PluginManager.h"
static PViewDataList *ViewData;
#endif

Tree_T *Symbol_T = NULL;

extern Context_T CTX;

static ExtrudeParams extr;

static gmshSurface *myGmshSurface = 0;

static List_T *ViewValueList = 0;
static double ViewCoord[100];
static int *ViewNumList = 0, ViewCoordIdx = 0;

#define MAX_RECUR_LOOPS 100
static int ImbricatedLoop = 0;
static fpos_t yyposImbricatedLoopsTab[MAX_RECUR_LOOPS];
static int yylinenoImbricatedLoopsTab[MAX_RECUR_LOOPS];
static double LoopControlVariablesTab[MAX_RECUR_LOOPS][3];
static char *LoopControlVariablesNameTab[MAX_RECUR_LOOPS];

void yyerror(char *s);
void yymsg(int level, const char *fmt, ...);
void skip_until(const char *skip, const char *until);
int PrintListOfDouble(char *format, List_T *list, char *buffer);
void FixRelativePath(const char *in, char *out);
%}

%union {
  char *c;
  int i;
  unsigned int u;
  double d;
  double v[5];
  Shape s;
  List_T *l;
}

%token <d> tDOUBLE
%token <c> tSTRING tBIGSTR

%token tEND tAFFECT tDOTS tPi tMPI_Rank tMPI_Size tEuclidian tCoordinates
%token tExp tLog tLog10 tSqrt tSin tAsin tCos tAcos tTan tRand
%token tAtan tAtan2 tSinh tCosh tTanh tFabs tFloor tCeil
%token tFmod tModulo tHypot 
%token tPrintf tSprintf tStrCat tStrPrefix tStrRelative
%token tBoundingBox tDraw tToday
%token tPoint tCircle tEllipse tLine tSphere tPolarSphere tSurface tSpline tVolume
%token tCharacteristic tLength tParametric tElliptic
%token tPlane tRuled tTransfinite tComplex tPhysical
%token tUsing tBump tProgression tPlugin
%token tRotate tTranslate tSymmetry tDilate tExtrude tDuplicata
%token tLoop tRecombine tSmoother tDelete tCoherence tIntersect tBoundary
%token tLayers tHole tAlias tAliasWithOptions
%token tText2D tText3D tInterpolationScheme  tTime tCombine
%token tBSpline tBezier tNurbs tOrder tKnots
%token tColor tColorTable tFor tIn tEndFor tIf tEndIf tExit
%token tField tReturn tCall tFunction tShow tHide tGetValue
%token tGMSH_MAJOR_VERSION tGMSH_MINOR_VERSION tGMSH_PATCH_VERSION

%type <d> FExpr FExpr_Single 
%type <v> VExpr VExpr_Single
%type <i> NumericAffectation NumericIncrement PhysicalId
%type <u> ColorExpr
%type <c> StringExpr StringExprVar SendToFile
%type <l> FExpr_Multi ListOfDouble RecursiveListOfDouble
%type <l> RecursiveListOfListOfDouble 
%type <l> ListOfColor RecursiveListOfColor 
%type <l> ListOfShapes Transform Extrude MultipleShape RuledSurfaceOptions
%type <s> Shape

// Operators (with ascending priority): cf. C language
//
// Notes: - associativity (%left, %right)
//        - UNARYPREC is a dummy terminal to resolve ambiguous cases
//          for + and - (which exist in both unary and binary form)

%right   tAFFECT tAFFECTPLUS tAFFECTMINUS tAFFECTTIMES tAFFECTDIVIDE
%right   '?' tDOTS
%left    tOR
%left    tAND
%left    tEQUAL tNOTEQUAL
%left    '<' tLESSOREQUAL  '>' tGREATEROREQUAL
%left    '+' '-'
%left    '*' '/' '%'
%right   '!' tPLUSPLUS tMINUSMINUS UNARYPREC
%right   '^'
%left    '(' ')' '[' ']' '.' '#'

%start All

%%

All : 
    GeoFormatItems
  | error tEND { yyerrok; return 1; }
;

//  G E O   F I L E   F O R M A T

GeoFormatItems : 
    // nothing
  | GeoFormatItems GeoFormatItem
;

GeoFormatItem :
    View        { return 1; }
  | Printf      { return 1; }
  | Affectation { return 1; }
  | Shape       { return 1; }
  | Transform   { List_Delete($1); return 1; }
  | Delete      { return 1; }
  | Colorify    { return 1; }
  | Visibility  { return 1; }
  | Extrude     { List_Delete($1); return 1; }
  | Transfinite { return 1; }
  | Embedding   { return 1; }
  | Coherence   { return 1; }
  | Loop        { return 1; }
  | Command     { return 1; }
;

SendToFile :
    '>'
    {
      $$ = (char*)"w";
    }
  | '>' '>'
    {
      $$ = (char*)"a";
    }
;

Printf :
    tPrintf '(' tBIGSTR ')' tEND
    {
      Msg::Direct($3);
      Free($3);
    }
  | tPrintf '(' tBIGSTR ')' SendToFile StringExprVar tEND
    {
      char tmpstring[1024];
      FixRelativePath($6, tmpstring);
      FILE *fp = fopen(tmpstring, $5);
      if(!fp){
	yymsg(0, "Unable to open file '%s'", tmpstring);
      }
      else{
	fprintf(fp, "%s\n", $3);
	fclose(fp);
      }
      Free($3);
      Free($6);
    }
  | tPrintf '(' tBIGSTR ',' RecursiveListOfDouble ')' tEND
    {
      char tmpstring[1024];
      int i = PrintListOfDouble($3, $5, tmpstring);
      if(i < 0) 
	yymsg(0, "Too few arguments in Printf");
      else if(i > 0)
	yymsg(0, "%d extra argument%s in Printf", i, (i > 1) ? "s" : "");
      else
	Msg::Direct(tmpstring);
      Free($3);
      List_Delete($5);
    }
  | tPrintf '(' tBIGSTR ',' RecursiveListOfDouble ')' SendToFile StringExprVar tEND
    {
      char tmpstring[1024];
      int i = PrintListOfDouble($3, $5, tmpstring);
      if(i < 0) 
	yymsg(0, "Too few arguments in Printf");
      else if(i > 0)
	yymsg(0, "%d extra argument%s in Printf", i, (i > 1) ? "s" : "");
      else{
	char tmpstring2[1024];
	FixRelativePath($8, tmpstring2);
	FILE *fp = fopen(tmpstring2, $7);
	if(!fp){
	  yymsg(0, "Unable to open file '%s'", tmpstring2);
	}
	else{
	  fprintf(fp, "%s\n", tmpstring);
	  fclose(fp);
	}
      }
      Free($3);
      Free($8);
      List_Delete($5);
    }
;

// V I E W 

View :
    tSTRING tBIGSTR '{' Views '}' tEND
    { 
#if !defined(HAVE_NO_POST)
      if(!strcmp($1, "View") && ViewData->finalize()){
	ViewData->setName($2);
	ViewData->setFileName(gmsh_yyname);
	ViewData->setFileIndex(gmsh_yyviewindex++);
	new PView(ViewData);
      }
      else
	delete ViewData;
#endif
      Free($1); Free($2);
    }
  | tAlias tSTRING '[' FExpr ']' tEND
    {
#if !defined(HAVE_NO_POST)
      if(!strcmp($2, "View")){
	int index = (int)$4;
	if(index >= 0 && index < (int)PView::list.size())
	  new PView(PView::list[index], false);
      }
#endif
      Free($2);
    }
  | tAliasWithOptions tSTRING '[' FExpr ']' tEND
    {
#if !defined(HAVE_NO_POST)
      if(!strcmp($2, "View")){
	int index = (int)$4;
	if(index >= 0 && index < (int)PView::list.size())
	  new PView(PView::list[index], true);
      }
#endif
      Free($2);
    }
;

Views :
    // nothing
    {
#if !defined(HAVE_NO_POST)
      ViewData = new PViewDataList(true); 
#endif
    }
  | Views Element
  | Views Text2D
  | Views Text3D
  | Views InterpolationMatrix
  | Views Time
;

ElementCoords :
    FExpr
    { ViewCoord[ViewCoordIdx++] = $1; }
  | ElementCoords ',' FExpr
    { ViewCoord[ViewCoordIdx++] = $3; }
;

ElementValues :
    FExpr
    { if(ViewValueList) List_Add(ViewValueList, &$1); }
  | ElementValues ',' FExpr
    { if(ViewValueList) List_Add(ViewValueList, &$3); }
;

Element : 
    tSTRING 
    {
#if !defined(HAVE_NO_POST)
      if(!strcmp($1, "SP")){
	ViewValueList = ViewData->SP; ViewNumList = &ViewData->NbSP;
      }
      else if(!strcmp($1, "VP")){
	ViewValueList = ViewData->VP; ViewNumList = &ViewData->NbVP;
      }
      else if(!strcmp($1, "TP")){
	ViewValueList = ViewData->TP; ViewNumList = &ViewData->NbTP;
      }
      else if(!strcmp($1, "SL")){
	ViewValueList = ViewData->SL; ViewNumList = &ViewData->NbSL;
      }
      else if(!strcmp($1, "VL")){
	ViewValueList = ViewData->VL; ViewNumList = &ViewData->NbVL;
      }
      else if(!strcmp($1, "TL")){
	ViewValueList = ViewData->TL; ViewNumList = &ViewData->NbTL;
      }
      else if(!strcmp($1, "ST")){
	ViewValueList = ViewData->ST; ViewNumList = &ViewData->NbST;
      }
      else if(!strcmp($1, "VT")){
	ViewValueList = ViewData->VT; ViewNumList = &ViewData->NbVT;
      }
      else if(!strcmp($1, "TT")){
	ViewValueList = ViewData->TT; ViewNumList = &ViewData->NbTT;
      }
      else if(!strcmp($1, "SQ")){
	ViewValueList = ViewData->SQ; ViewNumList = &ViewData->NbSQ;
      }
      else if(!strcmp($1, "VQ")){
	ViewValueList = ViewData->VQ; ViewNumList = &ViewData->NbVQ;
      }
      else if(!strcmp($1, "TQ")){
	ViewValueList = ViewData->TQ; ViewNumList = &ViewData->NbTQ;
      }
      else if(!strcmp($1, "SS")){
	ViewValueList = ViewData->SS; ViewNumList = &ViewData->NbSS;
      }
      else if(!strcmp($1, "VS")){
	ViewValueList = ViewData->VS; ViewNumList = &ViewData->NbVS;
      }
      else if(!strcmp($1, "TS")){
	ViewValueList = ViewData->TS; ViewNumList = &ViewData->NbTS;
      }
      else if(!strcmp($1, "SH")){
	ViewValueList = ViewData->SH; ViewNumList = &ViewData->NbSH;
      }
      else if(!strcmp($1, "VH")){
	ViewValueList = ViewData->VH; ViewNumList = &ViewData->NbVH;
      }
      else if(!strcmp($1, "TH")){
	ViewValueList = ViewData->TH; ViewNumList = &ViewData->NbTH;
      }
      else if(!strcmp($1, "SI")){
	ViewValueList = ViewData->SI; ViewNumList = &ViewData->NbSI;
      }
      else if(!strcmp($1, "VI")){
	ViewValueList = ViewData->VI; ViewNumList = &ViewData->NbVI;
      }
      else if(!strcmp($1, "TI")){
	ViewValueList = ViewData->TI; ViewNumList = &ViewData->NbTI;
      }
      else if(!strcmp($1, "SY")){
	ViewValueList = ViewData->SY; ViewNumList = &ViewData->NbSY;
      }
      else if(!strcmp($1, "VY")){
	ViewValueList = ViewData->VY; ViewNumList = &ViewData->NbVY;
      }
      else if(!strcmp($1, "TY")){
	ViewValueList = ViewData->TY; ViewNumList = &ViewData->NbTY;
      }
      else if(!strcmp($1, "SL2")){
	ViewValueList = ViewData->SL2; ViewNumList = &ViewData->NbSL2;
      }
      else if(!strcmp($1, "VL2")){
	ViewValueList = ViewData->VL2; ViewNumList = &ViewData->NbVL2;
      }
      else if(!strcmp($1, "TL2")){
	ViewValueList = ViewData->TL2; ViewNumList = &ViewData->NbTL2;
      }
      else if(!strcmp($1, "ST2")){
	ViewValueList = ViewData->ST2; ViewNumList = &ViewData->NbST2;
      }
      else if(!strcmp($1, "VT2")){
	ViewValueList = ViewData->VT2; ViewNumList = &ViewData->NbVT2;
      }
      else if(!strcmp($1, "TT2")){
	ViewValueList = ViewData->TT2; ViewNumList = &ViewData->NbTT2;
      }
      else if(!strcmp($1, "SQ2")){
	ViewValueList = ViewData->SQ2; ViewNumList = &ViewData->NbSQ2;
      }
      else if(!strcmp($1, "VQ2")){
	ViewValueList = ViewData->VQ2; ViewNumList = &ViewData->NbVQ2;
      }
      else if(!strcmp($1, "TQ2")){
	ViewValueList = ViewData->TQ2; ViewNumList = &ViewData->NbTQ2;
      }
      else if(!strcmp($1, "SS2")){
	ViewValueList = ViewData->SS2; ViewNumList = &ViewData->NbSS2;
      }
      else if(!strcmp($1, "VS2")){
	ViewValueList = ViewData->VS2; ViewNumList = &ViewData->NbVS2;
      }
      else if(!strcmp($1, "TS2")){
	ViewValueList = ViewData->TS2; ViewNumList = &ViewData->NbTS2;
      }
      else if(!strcmp($1, "SH2")){
	ViewValueList = ViewData->SH2; ViewNumList = &ViewData->NbSH2;
      }
      else if(!strcmp($1, "VH2")){
	ViewValueList = ViewData->VH2; ViewNumList = &ViewData->NbVH2;
      }
      else if(!strcmp($1, "TH2")){
	ViewValueList = ViewData->TH2; ViewNumList = &ViewData->NbTH2;
      }
      else if(!strcmp($1, "SI2")){
	ViewValueList = ViewData->SI2; ViewNumList = &ViewData->NbSI2;
      }
      else if(!strcmp($1, "VI2")){
	ViewValueList = ViewData->VI2; ViewNumList = &ViewData->NbVI2;
      }
      else if(!strcmp($1, "TI2")){
	ViewValueList = ViewData->TI2; ViewNumList = &ViewData->NbTI2;
      }
      else if(!strcmp($1, "SY2")){
	ViewValueList = ViewData->SY2; ViewNumList = &ViewData->NbSY2;
      }
      else if(!strcmp($1, "VY2")){
	ViewValueList = ViewData->VY2; ViewNumList = &ViewData->NbVY2;
      }
      else if(!strcmp($1, "TY2")){
	ViewValueList = ViewData->TY2; ViewNumList = &ViewData->NbTY2;
      }
      else{
	yymsg(0, "Unknown element type '%s'", $1);	
	ViewValueList = 0; ViewNumList = 0;
      }
#endif
      ViewCoordIdx = 0;
      Free($1);
    }
    '(' ElementCoords ')'
    {
#if !defined(HAVE_NO_POST)
      if(ViewValueList){
	for(int i = 0; i < 3; i++)
	  for(int j = 0; j < ViewCoordIdx / 3; j++)
	    List_Add(ViewValueList, &ViewCoord[3 * j + i]);
      }
#endif
    }
    '{' ElementValues '}' tEND
    {
#if !defined(HAVE_NO_POST)
      if(ViewValueList) (*ViewNumList)++;
#endif
    }
;

Text2DValues :
    StringExprVar
    { 
#if !defined(HAVE_NO_POST)
      for(int i = 0; i < (int)strlen($1)+1; i++) List_Add(ViewData->T2C, &$1[i]); 
#endif
      Free($1);
    }
  | Text2DValues ',' StringExprVar
    { 
#if !defined(HAVE_NO_POST)
      for(int i = 0; i < (int)strlen($3)+1; i++) List_Add(ViewData->T2C, &$3[i]); 
#endif
      Free($3);
    }
;

Text2D : 
    tText2D '(' FExpr ',' FExpr ',' FExpr ')'
    { 
#if !defined(HAVE_NO_POST)
      List_Add(ViewData->T2D, &$3); 
      List_Add(ViewData->T2D, &$5);
      List_Add(ViewData->T2D, &$7); 
      double d = List_Nbr(ViewData->T2C);
      List_Add(ViewData->T2D, &d); 
#endif
    }
    '{' Text2DValues '}' tEND
    {
#if !defined(HAVE_NO_POST)
      ViewData->NbT2++;
#endif
    }
;

Text3DValues :
    StringExprVar
    { 
#if !defined(HAVE_NO_POST)
      for(int i = 0; i < (int)strlen($1)+1; i++) List_Add(ViewData->T3C, &$1[i]); 
#endif
      Free($1);
    }
  | Text3DValues ',' StringExprVar
    { 
#if !defined(HAVE_NO_POST)
      for(int i = 0; i < (int)strlen($3)+1; i++) List_Add(ViewData->T3C, &$3[i]); 
#endif
      Free($3);
    }
;

Text3D : 
    tText3D '(' FExpr ',' FExpr ',' FExpr ',' FExpr ')'
    { 
#if !defined(HAVE_NO_POST)
      List_Add(ViewData->T3D, &$3); List_Add(ViewData->T3D, &$5);
      List_Add(ViewData->T3D, &$7); List_Add(ViewData->T3D, &$9); 
      double d = List_Nbr(ViewData->T3C);
      List_Add(ViewData->T3D, &d); 
#endif
    }
    '{' Text3DValues '}' tEND
    {
#if !defined(HAVE_NO_POST)
      ViewData->NbT3++;
#endif
    }
;

InterpolationMatrix :
    tInterpolationScheme '{' RecursiveListOfListOfDouble '}' 
                         '{' RecursiveListOfListOfDouble '}'  tEND
    {
#if !defined(HAVE_NO_POST)
      int type = 
	(ViewData->NbSL || ViewData->NbVL) ? 1 : 
	(ViewData->NbST || ViewData->NbVT) ? 3 : 
	(ViewData->NbSQ || ViewData->NbVQ) ? 4 : 
	(ViewData->NbSS || ViewData->NbVS) ? 6 : 
	(ViewData->NbSI || ViewData->NbVI) ? 9 : 
      	(ViewData->NbSH || ViewData->NbVH) ? 12 : 
	0;
      ViewData->setInterpolationScheme(type, $3, $6);
#endif
    }
 |  tInterpolationScheme '{' RecursiveListOfListOfDouble '}' 
                         '{' RecursiveListOfListOfDouble '}'  
                         '{' RecursiveListOfListOfDouble '}'  
                         '{' RecursiveListOfListOfDouble '}'  tEND
    {
#if !defined(HAVE_NO_POST)
      int type = 
	(ViewData->NbSL || ViewData->NbVL) ? 1 : 
	(ViewData->NbST || ViewData->NbVT) ? 3 : 
	(ViewData->NbSQ || ViewData->NbVQ) ? 4 : 
	(ViewData->NbSS || ViewData->NbVS) ? 6 : 
      	(ViewData->NbSH || ViewData->NbVH) ? 12 : 
	0;
      ViewData->setInterpolationScheme(type, $3, $6, $9, $12);
#endif
    }
;

Time :
    tTime 
    {
#if !defined(HAVE_NO_POST)
      ViewValueList = ViewData->Time;
#endif
    }
   '{' ElementValues '}' tEND
    {
    }
;

//  A F F E C T A T I O N

NumericAffectation :
    tAFFECT        { $$ = 0; }
  | tAFFECTPLUS    { $$ = 1; }
  | tAFFECTMINUS   { $$ = 2; }
  | tAFFECTTIMES   { $$ = 3; }
  | tAFFECTDIVIDE  { $$ = 4; }
;

NumericIncrement :
    tPLUSPLUS      { $$ = 1; }
  | tMINUSMINUS    { $$ = -1; }
;

Affectation :

  // Variables

    tSTRING NumericAffectation FExpr tEND
    {
      Symbol TheSymbol, *pSymbol;
      TheSymbol.Name = $1;
      if(!(pSymbol = (Symbol*)Tree_PQuery(Symbol_T, &TheSymbol))){
	if(!$2){
	  TheSymbol.val = List_Create(1, 1, sizeof(double));
	  List_Put(TheSymbol.val, 0, &$3);
	  Tree_Add(Symbol_T, &TheSymbol);
	}
	else{
	  yymsg(0, "Unknown variable '%s'", $1);
	  Free($1);
	}
      }
      else{
	double *pd = (double*)List_Pointer_Fast(pSymbol->val, 0); 
	switch($2){
	case 0 : *pd = $3; break;
	case 1 : *pd += $3; break;
	case 2 : *pd -= $3; break;
	case 3 : *pd *= $3; break;
	case 4 : 
	  if($3) *pd /= $3; 
	  else yymsg(0, "Division by zero in '%s /= %g'", $1, $3);
	  break;
	}
	Free($1);
      }
    }
  | tSTRING '[' FExpr ']' NumericAffectation FExpr tEND
    {
      Symbol TheSymbol, *pSymbol;
      TheSymbol.Name = $1;
      if(!(pSymbol = (Symbol*)Tree_PQuery(Symbol_T, &TheSymbol))){
	if(!$5){
	  TheSymbol.val = List_Create(5, 5, sizeof(double));
	  List_Put(TheSymbol.val, (int)$3, &$6);
	  Tree_Add(Symbol_T, &TheSymbol);
	}
	else{
	  yymsg(0, "Unknown variable '%s'", $1);
	  Free($1);
	}
      }
      else{
	double *pd;
	if((pd = (double*)List_Pointer_Test(pSymbol->val, (int)$3))){
	  switch($5){
	  case 0 : *pd = $6; break;
	  case 1 : *pd += $6; break;
	  case 2 : *pd -= $6; break;
	  case 3 : *pd *= $6; break;
	  case 4 : 
	    if($6) *pd /= $6; 
	    else yymsg(0, "Division by zero in '%s[%d] /= %g'", $1, (int)$3, $6);
	    break;
	  }
	}
	else{
	  if(!$5)
	    List_Put(pSymbol->val, (int)$3, &$6);
	  else
	    yymsg(0, "Uninitialized variable '%s[%d]'", $1, (int)$3);
	}
	Free($1);
      }
    }
  | tSTRING '[' '{' RecursiveListOfDouble '}' ']' NumericAffectation ListOfDouble tEND
    {
      if(List_Nbr($4) != List_Nbr($8)){
	yymsg(0, "Incompatible array dimensions in affectation");
	Free($1);
      }
      else{
	Symbol TheSymbol, *pSymbol;
	TheSymbol.Name = $1;
	if(!(pSymbol = (Symbol*)Tree_PQuery(Symbol_T, &TheSymbol))){
	  if(!$7){
	    TheSymbol.val = List_Create(5, 5, sizeof(double));
	    for(int i = 0; i < List_Nbr($4); i++){
	      List_Put(TheSymbol.val, (int)(*(double*)List_Pointer($4, i)),
		       (double*)List_Pointer($8, i));
	    }
	    Tree_Add(Symbol_T, &TheSymbol);
	  }
	  else{
	    yymsg(0, "Unknown variable '%s'", $1);
	    Free($1);
	  }
	}
	else{
	  for(int i = 0; i < List_Nbr($4); i++){
	    int j = (int)(*(double*)List_Pointer($4, i));
	    double d = *(double*)List_Pointer($8, i);
	    double *pd;
	    if((pd = (double*)List_Pointer_Test(pSymbol->val, j))){
	      switch($7){
	      case 0 : *pd = d; break;
	      case 1 : *pd += d; break;
	      case 2 : *pd -= d; break;
	      case 3 : *pd *= d; break;
	      case 4 : 
		if($8) *pd /= d; 
		else yymsg(0, "Division by zero in '%s[%d] /= %g'", $1, j, d);
		break;
	      }
	    }
	    else{
	      if(!$7)
		List_Put(pSymbol->val, j, &d);
	      else
		yymsg(0, "Uninitialized variable '%s[%d]'", $1, j);	  
	    }
	  }
	  Free($1);
	}
      }
      List_Delete($4);
      List_Delete($8);
    }
  | tSTRING '[' ']' tAFFECT ListOfDouble tEND
    {
      Symbol TheSymbol, *pSymbol;
      TheSymbol.Name = $1;
      if(!(pSymbol = (Symbol*)Tree_PQuery(Symbol_T, &TheSymbol))){
	TheSymbol.val = List_Create(5, 5, sizeof(double));
	List_Copy($5, TheSymbol.val);
	Tree_Add(Symbol_T, &TheSymbol);
      }
      else{
	List_Reset(pSymbol->val);
	List_Copy($5, pSymbol->val);
	Free($1);
      }
      List_Delete($5);
    }
  | tSTRING '[' ']' tAFFECTPLUS ListOfDouble tEND
    {
      // appends to the list
      Symbol TheSymbol, *pSymbol;
      TheSymbol.Name = $1;
      if(!(pSymbol = (Symbol*)Tree_PQuery(Symbol_T, &TheSymbol))){
	TheSymbol.val = List_Create(5, 5, sizeof(double));
	List_Copy($5, TheSymbol.val);
	Tree_Add(Symbol_T, &TheSymbol);
      }
      else{
	for(int i = 0; i < List_Nbr($5); i++)
	  List_Add(pSymbol->val, List_Pointer($5, i));
	Free($1);
      }
      List_Delete($5);
    }
  | tSTRING NumericIncrement tEND
    {
      Symbol TheSymbol, *pSymbol;
      TheSymbol.Name = $1;
      if(!(pSymbol = (Symbol*)Tree_PQuery(Symbol_T, &TheSymbol)))
	yymsg(0, "Unknown variable '%s'", $1); 
      else
	*(double*)List_Pointer_Fast(pSymbol->val, 0) += $2;
      Free($1);
    }
  | tSTRING '[' FExpr ']' NumericIncrement tEND
    {
      Symbol TheSymbol, *pSymbol;
      TheSymbol.Name = $1;
      if(!(pSymbol = (Symbol*)Tree_PQuery(Symbol_T, &TheSymbol)))
	yymsg(0, "Unknown variable '%s'", $1); 
      else{
	double *pd;
	if((pd = (double*)List_Pointer_Test(pSymbol->val, (int)$3)))
	  *pd += $5;
	else
	  yymsg(0, "Uninitialized variable '%s[%d]'", $1, (int)$3);
      }
      Free($1);
    }

  | tSTRING tAFFECT StringExpr tEND 
    { 
      Msg::Warning("Named string expressions not implemented yet");
    }

  // Option Strings

  | tSTRING '.' tSTRING tAFFECT StringExpr tEND 
    { 
      StringOption(GMSH_SET|GMSH_GUI, $1, 0, $3, $5);
      Free($1); Free($3);
    }
  | tSTRING '[' FExpr ']' '.' tSTRING tAFFECT StringExpr tEND 
    { 
      StringOption(GMSH_SET|GMSH_GUI, $1, (int)$3, $6, $8);
      Free($1); Free($6);
    }

  // Option Numbers

  | tSTRING '.' tSTRING NumericAffectation FExpr tEND 
    {
      double d = 0.;
      if(NumberOption(GMSH_GET, $1, 0, $3, d)){
	switch($4){
	case 0 : d = $5; break;
	case 1 : d += $5; break;
	case 2 : d -= $5; break;
	case 3 : d *= $5; break;
	case 4 : 
	  if($5) d /= $5; 
	  else yymsg(0, "Division by zero in '%s.%s /= %g'", $1, $3, $5);
	  break;
	}
	NumberOption(GMSH_SET|GMSH_GUI, $1, 0, $3, d);
      }
      Free($1); Free($3);
    }
  | tSTRING '[' FExpr ']' '.' tSTRING NumericAffectation FExpr tEND 
    {
      double d = 0.;
      if(NumberOption(GMSH_GET, $1, (int)$3, $6, d)){
	switch($7){
	case 0 : d = $8; break;
	case 1 : d += $8; break;
	case 2 : d -= $8; break;
	case 3 : d *= $8; break;
	case 4 : 
	  if($8) d /= $8; 
	  else yymsg(0, "Division by zero in '%s[%d].%s /= %g'", $1, (int)$3, $6, $8);
	  break;
	}
	NumberOption(GMSH_SET|GMSH_GUI, $1, (int)$3, $6, d);
      }
      Free($1); Free($6);
    }
  | tSTRING '.' tSTRING NumericIncrement tEND 
    {
      double d = 0.;
      if(NumberOption(GMSH_GET, $1, 0, $3, d)){
	d += $4;
	NumberOption(GMSH_SET|GMSH_GUI, $1, 0, $3, d);
      }
      Free($1); Free($3);
    }
  | tSTRING '[' FExpr ']' '.' tSTRING NumericIncrement tEND 
    {
      double d = 0.;
      if(NumberOption(GMSH_GET, $1, (int)$3, $6, d)){
	d += $7;
	NumberOption(GMSH_SET|GMSH_GUI, $1, (int)$3, $6, d);
      }
      Free($1); Free($6);
    }

  // Option Colors

  | tSTRING '.' tColor '.' tSTRING tAFFECT ColorExpr tEND 
    {
      ColorOption(GMSH_SET|GMSH_GUI, $1, 0, $5, $7);
      Free($1); Free($5);
    }
  | tSTRING '[' FExpr ']' '.' tColor '.' tSTRING tAFFECT ColorExpr tEND 
    {
      ColorOption(GMSH_SET|GMSH_GUI, $1, (int)$3, $8, $10);
      Free($1); Free($8);
    }

  // ColorTable

  | tSTRING '.' tColorTable tAFFECT ListOfColor tEND 
    {
      GmshColorTable *ct = Get_ColorTable(0);
      if(!ct)
	yymsg(0, "View[%d] does not exist", 0);
      else{
	ct->size = List_Nbr($5);
	if(ct->size > COLORTABLE_NBMAX_COLOR)
	  yymsg(0, "Too many (%d>%d) colors in View[%d].ColorTable", 
		ct->size, COLORTABLE_NBMAX_COLOR, 0);
	else
	  for(int i = 0; i < ct->size; i++) List_Read($5, i, &ct->table[i]);
	if(ct->size == 1){
	  ct->size = 2;
	  ct->table[1] = ct->table[0];
	}
      }
      Free($1);
      List_Delete($5);
    }
  | tSTRING '[' FExpr ']' '.' tColorTable tAFFECT ListOfColor tEND 
    {
      GmshColorTable *ct = Get_ColorTable((int)$3);
      if(!ct)
	yymsg(0, "View[%d] does not exist", (int)$3);
      else{
	ct->size = List_Nbr($8);
	if(ct->size > COLORTABLE_NBMAX_COLOR)
	  yymsg(0, "Too many (%d>%d) colors in View[%d].ColorTable", 
		   ct->size, COLORTABLE_NBMAX_COLOR, (int)$3);
	else
	  for(int i = 0; i < ct->size; i++) List_Read($8, i, &ct->table[i]);
	if(ct->size == 1){
	  ct->size = 2;
	  ct->table[1] = ct->table[0];
	}
      }
      Free($1);
      List_Delete($8);
    }

  // Fields

  | tSTRING tField tAFFECT FExpr tEND
    {
      if(!strcmp($1,"Background")){
	GModel::current()->getFields()->background_field = (int)$4;
      }
      else{
	yymsg(0, "Unknown command %s Field", $1);
      }
    }
  | tField '[' FExpr ']' tAFFECT tSTRING tEND
    {
      if(!GModel::current()->getFields()->new_field((int)$3, $6))
	yymsg(0, "Cannot create field %i of type '%s'", (int)$3, $6);
      Free($6);
    }
  | tField '[' FExpr ']' '.' tSTRING  tAFFECT FExpr tEND
    {
      Field *field = GModel::current()->getFields()->get((int)$3);
      if(field){
	FieldOption *option = field->options[$6];
	if(option){
	  try { option->numerical_value($8); }
	  catch(...){
	    yymsg(0, "Cannot assign a numerical value to option '%s' "
		  "in field %i of type '%s'", $6, (int)$3, field->get_name());
	  }
	}
	else
	  yymsg(0, "Unknown option '%s' in field %i of type '%s'",
		$6, (int)$3, field->get_name());
      }
      else
	yymsg(0, "No field with id %i", (int)$3);
      Free($6);
    }
  | tField '[' FExpr ']' '.' tSTRING  tAFFECT StringExpr tEND
    {
      Field *field = GModel::current()->getFields()->get((int)$3);
      if(field){
	FieldOption *option = field->options[$6];
	if(option){
	  try { option->string() = $8; }
	  catch (...){
	    yymsg(0, "Cannot assign a string value to  option '%s' "
		  "in field %i of type '%s'", $6, (int)$3, field->get_name());
	  }
	}
	else 
	  yymsg(0, "Unknown option '%s' in field %i of type '%s'", 
		$6, (int)$3, field->get_name());
      }
      else 
	yymsg(0, "No field with id %i", (int)$3);
      Free($6);
      Free($8);
    }
  | tField '[' FExpr ']' '.' tSTRING  tAFFECT '{' RecursiveListOfDouble '}' tEND
    {
      Field *field = GModel::current()->getFields()->get((int)$3);
      if(field){
	FieldOption *option = field->options[$6];
	if(option){
	  std::list<int> &vl = option->list();
	  vl.clear();
	  for(int i = 0; i < List_Nbr($9); i++){
	    double id;
	    List_Read($9, i, &id);
	    vl.push_back((int)id);
	  }
	}
	else
	  yymsg(0, "Unknown option '%s' in field %i of type '%s'",
		$6, (int)$3, field->get_name());
      }
      else 
	yymsg(0, "No field with id %i", (int)$3);
      Free($6);
      List_Delete($9);
    }

  // Plugins

  | tPlugin '(' tSTRING ')' '.' tSTRING tAFFECT FExpr tEND 
    {
#if !defined(HAVE_NO_POST)
      try {
	GMSH_PluginManager::instance()->setPluginOption($3, $6, $8); 
      }
      catch (...) {
	yymsg(0, "Unknown option '%s' or plugin '%s'", $6, $3);
      }
#endif
      Free($3); Free($6);
    }
  | tPlugin '(' tSTRING ')' '.' tSTRING tAFFECT StringExpr tEND 
    {
#if !defined(HAVE_NO_POST)
      try {
	GMSH_PluginManager::instance()->setPluginOption($3, $6, $8); 
      }
      catch (...) {
	yymsg(0, "Unknown option '%s' or plugin '%s'", $6, $3);
      }
#endif
      Free($3); Free($6);
    }
;

//  S H A P E

PhysicalId :
    FExpr
    { 
      $$ = (int)$1; 
    }
  | StringExpr
    { 
      $$ = GModel::current()->setPhysicalName
	(std::string($1), ++GModel::current()->getGEOInternals()->MaxPhysicalNum);
      Free($1);
    }
;

RuledSurfaceOptions :
    // nothing
    {
      $$ = 0;
    }
  | tIn tSphere '{' FExpr '}'
    {
      $$ = List_Create(4, 4, sizeof(double));
      Vertex *v = FindPoint((int)$4);
      if(!v)
	yymsg(0, "Unknown point %d", (int)$4);
      else{
	List_Add($$, &v->Pos.X);
	List_Add($$, &v->Pos.Y);
	List_Add($$, &v->Pos.Z);
      }
    }
;

Shape :

  // Points

    tPoint '(' FExpr ')' tAFFECT VExpr tEND
    {
      int num = (int)$3;
      if(FindPoint(num)){
	yymsg(0, "Point %d already exists", num);
      }
      else{
	double x = CTX.geom.scaling_factor * $6[0];
	double y = CTX.geom.scaling_factor * $6[1];
	double z = CTX.geom.scaling_factor * $6[2];
	double lc = CTX.geom.scaling_factor * $6[3];
	Vertex *v;
	if(!myGmshSurface)
	  v = Create_Vertex(num, x, y, z, lc, 1.0);
	else
	  v = Create_Vertex(num, x, y, myGmshSurface, lc);
	Tree_Add(GModel::current()->getGEOInternals()->Points, &v);
	AddToTemporaryBoundingBox(v->Pos.X, v->Pos.Y, v->Pos.Z);
      }
      $$.Type = MSH_POINT;
      $$.Num = num;
    }
  | tPhysical tPoint '(' PhysicalId ')' tAFFECT ListOfDouble tEND
    {
      int num = (int)$4;
      if(FindPhysicalGroup(num, MSH_PHYSICAL_POINT)){
	yymsg(0, "Physical point %d already exists", num);
      }
      else{
	List_T *temp = ListOfDouble2ListOfInt($7);
	PhysicalGroup *p = Create_PhysicalGroup(num, MSH_PHYSICAL_POINT, temp);
	List_Delete(temp);
	List_Add(GModel::current()->getGEOInternals()->PhysicalGroups, &p);
      }
      List_Delete($7);
      $$.Type = MSH_PHYSICAL_POINT;
      $$.Num = num;
    }
  | tCharacteristic tLength ListOfDouble tAFFECT FExpr tEND
    {      
      for(int i = 0; i < List_Nbr($3); i++){
	double d;
	List_Read($3, i, &d);
	Vertex *v = FindPoint((int)d); 	 
	if(v)
	  v->lc = $5;
	else{
	  GVertex *gv = GModel::current()->getVertexByTag((int)d);
	  if(gv) 
	    gv->setPrescribedMeshSizeAtVertex($5);
	}
      }
      List_Delete($3);
      // dummy values
      $$.Type = 0;
      $$.Num = 0;
    }  

  // Lines

  | tLine '(' FExpr ')' tAFFECT ListOfDouble tEND
    {
      int num = (int)$3;
      if(FindCurve(num)){
	yymsg(0, "Curve %d already exists", num);
      }
      else{
	List_T *temp = ListOfDouble2ListOfInt($6);
	Curve *c = Create_Curve(num, MSH_SEGM_LINE, 1, temp, NULL,
				-1, -1, 0., 1.);
	Tree_Add(GModel::current()->getGEOInternals()->Curves, &c);
	CreateReversedCurve(c);
	List_Delete(temp);
      }
      List_Delete($6);
      $$.Type = MSH_SEGM_LINE;
      $$.Num = num;
    }
  | tSpline '(' FExpr ')' tAFFECT ListOfDouble tEND
    {
      int num = (int)$3;
      if(FindCurve(num)){
	yymsg(0, "Curve %d already exists", num);
      }
      else{
	List_T *temp = ListOfDouble2ListOfInt($6);
	Curve *c = Create_Curve(num, MSH_SEGM_SPLN, 3, temp, NULL,
				-1, -1, 0., 1.);
	Tree_Add(GModel::current()->getGEOInternals()->Curves, &c);
	CreateReversedCurve(c);
	List_Delete(temp);
      }
      List_Delete($6);
      $$.Type = MSH_SEGM_SPLN;
      $$.Num = num;
    }
  | tCircle '(' FExpr ')'  tAFFECT ListOfDouble tEND
    {
      int num = (int)$3;
      if(FindCurve(num)){
	yymsg(0, "Curve %d already exists", num);
      }
      else{
	List_T *temp = ListOfDouble2ListOfInt($6);
	Curve *c = Create_Curve(num, MSH_SEGM_CIRC, 2, temp, NULL,
				-1, -1, 0., 1.);
	Tree_Add(GModel::current()->getGEOInternals()->Curves, &c);
	CreateReversedCurve(c);
	List_Delete(temp);
      }
      List_Delete($6);
      $$.Type = MSH_SEGM_CIRC;
      $$.Num = num;
    }
  | tCircle '(' FExpr ')'  tAFFECT ListOfDouble tPlane VExpr tEND
    {
      int num = (int)$3;
      if(FindCurve(num)){
	yymsg(0, "Curve %d already exists", num);
      }
      else{
	List_T *temp = ListOfDouble2ListOfInt($6);
	Curve *c = Create_Curve(num, MSH_SEGM_CIRC, 2, temp, NULL,
				-1, -1, 0., 1.);
	c->Circle.n[0] = $8[0];
	c->Circle.n[1] = $8[1];
	c->Circle.n[2] = $8[2];
	End_Curve(c);
	Tree_Add(GModel::current()->getGEOInternals()->Curves, &c);
	Curve *rc = CreateReversedCurve(c);
	rc->Circle.n[0] = $8[0];
	rc->Circle.n[1] = $8[1];
	rc->Circle.n[2] = $8[2];
	End_Curve(rc);
	List_Delete(temp);
      }
      List_Delete($6);
      $$.Type = MSH_SEGM_CIRC;
      $$.Num = num;
    }
  | tEllipse '(' FExpr ')'  tAFFECT ListOfDouble tEND
    {
      int num = (int)$3;
      if(FindCurve(num)){
	yymsg(0, "Curve %d already exists", num);
      }
      else{
	List_T *temp = ListOfDouble2ListOfInt($6);
	Curve *c = Create_Curve(num, MSH_SEGM_ELLI, 2, temp, NULL,
				-1, -1, 0., 1.);
	Tree_Add(GModel::current()->getGEOInternals()->Curves, &c);
	CreateReversedCurve(c);
	List_Delete(temp);
      }
      List_Delete($6);
      $$.Type = MSH_SEGM_ELLI;
      $$.Num = num;
    }
  | tEllipse '(' FExpr ')'  tAFFECT ListOfDouble tPlane VExpr tEND
    {
      int num = (int)$3;
      if(FindCurve(num)){
	yymsg(0, "Curve %d already exists", num);
      }
      else{
	List_T *temp = ListOfDouble2ListOfInt($6);
	Curve *c = Create_Curve(num, MSH_SEGM_ELLI, 2, temp, NULL,
				-1, -1, 0., 1.);
	c->Circle.n[0] = $8[0];
	c->Circle.n[1] = $8[1];
	c->Circle.n[2] = $8[2];
	End_Curve(c);
	Tree_Add(GModel::current()->getGEOInternals()->Curves, &c);
	Curve *rc = CreateReversedCurve(c);
	rc->Circle.n[0] = $8[0];
	rc->Circle.n[1] = $8[1];
	rc->Circle.n[2] = $8[2];
	End_Curve(c);
	List_Delete(temp);
      }
      List_Delete($6);
      $$.Type = MSH_SEGM_ELLI;
      $$.Num = num;
    }
  | tParametric '(' FExpr ')' tAFFECT 
      '{' FExpr ',' FExpr ',' tBIGSTR ',' tBIGSTR ',' tBIGSTR '}' tEND
    {
      int num = (int)$3;
      if(FindCurve(num)){
	yymsg(0, "Curve %d already exists", num);
      }
      else{
	Curve *c = Create_Curve(num, MSH_SEGM_PARAMETRIC, 2, NULL, NULL,
				-1, -1, $7, $9);
	strcpy(c->functu, $11);
	strcpy(c->functv, $13);
	strcpy(c->functw, $15);
	Tree_Add(GModel::current()->getGEOInternals()->Curves, &c);
	CreateReversedCurve(c);
      }
      Free($11); Free($13); Free($15);
      $$.Type = MSH_SEGM_PARAMETRIC;
      $$.Num = num;
    }
  | tBSpline '(' FExpr ')' tAFFECT ListOfDouble tEND
    {
      int num = (int)$3;
      if(FindCurve(num)){
	yymsg(0, "Curve %d already exists", num);
      }
      else{
	List_T *temp = ListOfDouble2ListOfInt($6);
	Curve *c = Create_Curve(num, MSH_SEGM_BSPLN, 2, temp, NULL,
				-1, -1, 0., 1.);
	Tree_Add(GModel::current()->getGEOInternals()->Curves, &c);
	CreateReversedCurve(c);
	List_Delete(temp);
      }
      List_Delete($6);
      $$.Type = MSH_SEGM_BSPLN;
      $$.Num = num;
    }
  | tBezier '(' FExpr ')' tAFFECT ListOfDouble tEND
    {
      int num = (int)$3;
      if(FindCurve(num)){
	yymsg(0, "Curve %d already exists", num);
      }
      else{
	List_T *temp = ListOfDouble2ListOfInt($6);
	Curve *c = Create_Curve(num, MSH_SEGM_BEZIER, 2, temp, NULL,
				-1, -1, 0., 1.);
	Tree_Add(GModel::current()->getGEOInternals()->Curves, &c);
	CreateReversedCurve(c);
	List_Delete(temp);
      }
      List_Delete($6);
      $$.Type = MSH_SEGM_BEZIER;
      $$.Num = num;
    }
  | tNurbs  '(' FExpr ')' tAFFECT ListOfDouble tKnots ListOfDouble tOrder FExpr tEND
    {
      int num = (int)$3;
      if(List_Nbr($6) + (int)$10 + 1 != List_Nbr($8)){
	yymsg(0, "Wrong definition of Nurbs Curve %d: "
	      "got %d knots, need N + D + 1 = %d + %d + 1 = %d",
	      (int)$3, List_Nbr($8), List_Nbr($6), (int)$10, List_Nbr($6) + (int)$10 + 1);
      }
      else{
	if(FindCurve(num)){
	  yymsg(0, "Curve %d already exists", num);
	}
	else{
	  List_T *temp = ListOfDouble2ListOfInt($6);
	  Curve *c = Create_Curve(num, MSH_SEGM_NURBS, (int)$10, temp, $8,
				  -1, -1, 0., 1.);
	  Tree_Add(GModel::current()->getGEOInternals()->Curves, &c);
	  CreateReversedCurve(c);
	  List_Delete(temp);
	}
      }
      List_Delete($6);
      List_Delete($8);
      $$.Type = MSH_SEGM_NURBS;
      $$.Num = num;
    }
  | tLine tLoop '(' FExpr ')' tAFFECT ListOfDouble tEND
    {
      int num = (int)$4;
      if(FindEdgeLoop(num)){
	yymsg(0, "Line loop %d already exists", num);
      }
      else{
	List_T *temp = ListOfDouble2ListOfInt($7);
	sortEdgesInLoop(num, temp);
	EdgeLoop *l = Create_EdgeLoop(num, temp);
	Tree_Add(GModel::current()->getGEOInternals()->EdgeLoops, &l);
	List_Delete(temp);
      }
      List_Delete($7);
      $$.Type = MSH_SEGM_LOOP;
      $$.Num = num;
    }
  | tPhysical tLine '(' PhysicalId ')' tAFFECT ListOfDouble tEND
    {
      int num = (int)$4;
      if(FindPhysicalGroup(num, MSH_PHYSICAL_LINE)){
	yymsg(0, "Physical line %d already exists", num);
      }
      else{
	List_T *temp = ListOfDouble2ListOfInt($7);
	PhysicalGroup *p = Create_PhysicalGroup(num, MSH_PHYSICAL_LINE, temp);
	List_Delete(temp);
	List_Add(GModel::current()->getGEOInternals()->PhysicalGroups, &p);
      }
      List_Delete($7);
      $$.Type = MSH_PHYSICAL_LINE;
      $$.Num = num;
    }

  // Surfaces

  | tPlane tSurface '(' FExpr ')' tAFFECT ListOfDouble tEND
    {
      int num = (int)$4;
      if(FindSurface(num)){
	yymsg(0, "Surface %d already exists", num);
      }
      else{
	Surface *s = Create_Surface(num, MSH_SURF_PLAN);
	List_T *temp = ListOfDouble2ListOfInt($7);
	setSurfaceGeneratrices(s, temp);
	List_Delete(temp);
	End_Surface(s);
	Tree_Add(GModel::current()->getGEOInternals()->Surfaces, &s);
      }
      List_Delete($7);
      $$.Type = MSH_SURF_PLAN;
      $$.Num = num;
    }
  | tRuled tSurface '(' FExpr ')' tAFFECT ListOfDouble RuledSurfaceOptions tEND
    {
      int num = (int)$4, type = 0;
      if(FindSurface(num)){
	yymsg(0, "Surface %d already exists", num);
      }
      else{
	double d;
	List_Read($7, 0, &d);
	EdgeLoop *el = FindEdgeLoop((int)fabs(d));
	if(!el){
	  yymsg(0, "Unknown line loop %d", (int)d);
	}
	else{
	  int j = List_Nbr(el->Curves);
	  if(j == 4){
	    type = MSH_SURF_REGL;
	  }
	  else if(j == 3){
	    type = MSH_SURF_TRIC;
	  }
	  else{
	    yymsg(0, "Wrong definition of Ruled Surface %d: "
		  "%d borders instead of 3 or 4", num, j);
	    type = MSH_SURF_PLAN;
	  }
	  Surface *s = Create_Surface(num, type);
	  List_T *temp = ListOfDouble2ListOfInt($7);
	  setSurfaceGeneratrices(s, temp);
	  List_Delete(temp);
	  End_Surface(s);
	  s->RuledSurfaceOptions = $8;
	  Tree_Add(GModel::current()->getGEOInternals()->Surfaces, &s);
	}
      }
      List_Delete($7);
      $$.Type = type;
      $$.Num = num;
    }
  | tEuclidian tCoordinates tEND
    {
      myGmshSurface = 0;
      $$.Type = 0;
      $$.Num = 0;
    }  
  | tCoordinates tSurface FExpr tEND
    {
      myGmshSurface = gmshSurface::getSurface((int)$3);
      $$.Type = 0;
      $$.Num = 0;
    }  
  | tParametric tSurface '(' FExpr ')' tAFFECT tBIGSTR tBIGSTR tBIGSTR tEND
    {
      int num = (int)$4;
      myGmshSurface = gmshParametricSurface::NewParametricSurface(num, $7, $8, $9);
      $$.Type = 0;
      $$.Num = num;
    }
  | tSphere '(' FExpr ')' tAFFECT ListOfDouble tEND
    {
      int num = (int)$3;
      if (List_Nbr($6) != 2){
	yymsg(0, "Sphere %d has to be defined using 2 points (center + "
	      "any point) and not %d", num, List_Nbr($6));
      }
      else{
	double p1,p2;
	List_Read($6, 0, &p1);
	List_Read($6, 1, &p2);
	Vertex *v1 = FindPoint((int)p1);
	Vertex *v2 = FindPoint((int)p2);
	if(!v1) yymsg(0, "Sphere %d : unknown point %d", num, (int)p1);
	if(!v2) yymsg(0, "Sphere %d : unknown point %d", num, (int)p2);
	if(v1 && v2)
	  myGmshSurface = gmshSphere::NewSphere
	    (num, v1->Pos.X, v1->Pos.Y, v1->Pos.Z,
	     sqrt((v2->Pos.X - v1->Pos.X) * (v2->Pos.X - v1->Pos.X) +
		  (v2->Pos.Y - v1->Pos.Y) * (v2->Pos.Y - v1->Pos.Y) +
		  (v2->Pos.Z - v1->Pos.Z) * (v2->Pos.Z - v1->Pos.Z)));
      }
      $$.Type = 0;
      $$.Num = num;
    }
  | tPolarSphere '(' FExpr ')' tAFFECT ListOfDouble tEND
    {
      int num = (int)$3;
      if (List_Nbr($6) != 2){
	yymsg(0, "PolarSphere %d has to be defined using 2 points (center + "
	      "any point) and not %d", num, List_Nbr($6));
      }
      else{
	double p1,p2;
	List_Read($6, 0, &p1);
	List_Read($6, 1, &p2);
	Vertex *v1 = FindPoint((int)p1);
	Vertex *v2 = FindPoint((int)p2);
	if(!v1) yymsg(0, "PolarSphere %d : unknown point %d", num, (int)p1);
	if(!v2) yymsg(0, "PolarSphere %d : unknown point %d", num, (int)p2);
	if(v1 && v2)
	  myGmshSurface = gmshPolarSphere::NewPolarSphere
	    (num, v1->Pos.X, v1->Pos.Y, v1->Pos.Z,
	     sqrt((v2->Pos.X - v1->Pos.X) * (v2->Pos.X - v1->Pos.X) +
		  (v2->Pos.Y - v1->Pos.Y) * (v2->Pos.Y - v1->Pos.Y) +
		  (v2->Pos.Z - v1->Pos.Z) * (v2->Pos.Z - v1->Pos.Z)));
      }
      $$.Type = 0;
      $$.Num = num;
    }
  | tSurface tLoop '(' FExpr ')' tAFFECT ListOfDouble tEND
    {
      int num = (int)$4;
      if(FindSurfaceLoop(num)){
	yymsg(0, "Surface loop %d already exists", num);
      }
      else{
	List_T *temp = ListOfDouble2ListOfInt($7);
	SurfaceLoop *l = Create_SurfaceLoop(num, temp);
	Tree_Add(GModel::current()->getGEOInternals()->SurfaceLoops, &l);
	List_Delete(temp);
      }
      List_Delete($7);
      $$.Type = MSH_SURF_LOOP;
      $$.Num = num;
    }
  | tPhysical tSurface '(' PhysicalId ')' tAFFECT ListOfDouble tEND
    {
      int num = (int)$4;
      if(FindPhysicalGroup(num, MSH_PHYSICAL_SURFACE)){
	yymsg(0, "Physical surface %d already exists", num);
      }
      else{
	List_T *temp = ListOfDouble2ListOfInt($7);
	PhysicalGroup *p = Create_PhysicalGroup(num, MSH_PHYSICAL_SURFACE, temp);
	List_Delete(temp);
	List_Add(GModel::current()->getGEOInternals()->PhysicalGroups, &p);
      }
      List_Delete($7);
      $$.Type = MSH_PHYSICAL_SURFACE;
      $$.Num = num;
    }

  // Volumes

  // for backward compatibility:
  | tComplex tVolume '(' FExpr ')' tAFFECT ListOfDouble tEND
    {
      yymsg(0, "'Complex Volume' command is deprecated: use 'Volume' instead");
      int num = (int)$4;
      if(FindVolume(num)){
	yymsg(0, "Volume %d already exists", num);
      }
      else{
	Volume *v = Create_Volume(num, MSH_VOLUME);
	List_T *temp = ListOfDouble2ListOfInt($7);
	setVolumeSurfaces(v, temp);
	List_Delete(temp);
	Tree_Add(GModel::current()->getGEOInternals()->Volumes, &v);
      }
      List_Delete($7);
      $$.Type = MSH_VOLUME;
      $$.Num = num;
    }
  | tVolume '(' FExpr ')' tAFFECT ListOfDouble tEND
    {
      int num = (int)$3;
      if(FindVolume(num)){
	yymsg(0, "Volume %d already exists", num);
      }
      else{
	Volume *v = Create_Volume(num, MSH_VOLUME);
	List_T *temp = ListOfDouble2ListOfInt($6);
	setVolumeSurfaces(v, temp);
	List_Delete(temp);
	Tree_Add(GModel::current()->getGEOInternals()->Volumes, &v);
      }
      List_Delete($6);
      $$.Type = MSH_VOLUME;
      $$.Num = num;
    }
  | tPhysical tVolume '(' PhysicalId ')' tAFFECT ListOfDouble tEND
    {
      int num = (int)$4;
      if(FindPhysicalGroup(num, MSH_PHYSICAL_VOLUME)){
	yymsg(0, "Physical volume %d already exists", num);
      }
      else{
	List_T *temp = ListOfDouble2ListOfInt($7);
	PhysicalGroup *p = Create_PhysicalGroup(num, MSH_PHYSICAL_VOLUME, temp);
	List_Delete(temp);
	List_Add(GModel::current()->getGEOInternals()->PhysicalGroups, &p);
      }
      List_Delete($7);
      $$.Type = MSH_PHYSICAL_VOLUME;
      $$.Num = num;
    }
;

//  T R A N S F O R M

Transform :
    tTranslate VExpr '{' MultipleShape '}'
    {
      TranslateShapes($2[0], $2[1], $2[2], $4);
      $$ = $4;
    }
  | tRotate '{' VExpr ',' VExpr ',' FExpr '}' '{' MultipleShape '}'
    {
      RotateShapes($3[0], $3[1], $3[2], $5[0], $5[1], $5[2], $7, $10);
      $$ = $10;
    }
  | tSymmetry  VExpr '{' MultipleShape '}'
    {
      SymmetryShapes($2[0], $2[1], $2[2], $2[3], $4);
      $$ = $4;
    }
  | tDilate '{' VExpr ',' FExpr '}' '{' MultipleShape '}'
    {
      DilatShapes($3[0], $3[1], $3[2], $5, $8);
      $$ = $8;
    }
  | tDuplicata '{' MultipleShape '}'
    {
      $$ = List_Create(3, 3, sizeof(Shape));
      for(int i = 0; i < List_Nbr($3); i++){
	Shape TheShape;
	List_Read($3, i, &TheShape);
	CopyShape(TheShape.Type, TheShape.Num, &TheShape.Num);
	List_Add($$, &TheShape);
      }
      List_Delete($3);
    }
  | tIntersect tLine '{' RecursiveListOfDouble '}' tSurface '{' FExpr '}' 
    { 
      $$ = List_Create(2, 1, sizeof(Shape));
      IntersectCurvesWithSurface($4, (int)$8, $$);
      List_Delete($4);
    }
  | tBoundary '{' MultipleShape '}'
    { 
      $$ = List_Create(2, 1, sizeof(Shape));
      BoundaryShapes($3, $$);
      List_Delete($3);
    }
;

MultipleShape : 
    ListOfShapes  { $$ = $1; }
  | Transform     { $$ = $1; }
;

ListOfShapes : 
    // nothing
    {
      $$ = List_Create(3, 3, sizeof(Shape));
    }   
  | ListOfShapes Shape
    {
      List_Add($$, &$2);
    }
  | ListOfShapes tPoint '{' RecursiveListOfDouble '}' tEND
    {
      for(int i = 0; i < List_Nbr($4); i++){
	double d;
	List_Read($4, i, &d);
	Shape TheShape;
	TheShape.Num = (int)d;
	Vertex *v = FindPoint(TheShape.Num);
	if(v){
	  TheShape.Type = MSH_POINT;
	  List_Add($$, &TheShape);
	}
	else{
	  GVertex *gv = GModel::current()->getVertexByTag(TheShape.Num);
	  if(gv){
	    TheShape.Type = MSH_POINT_FROM_GMODEL;
	    List_Add($$, &TheShape);
	  }
	  else
	    yymsg(1, "Unknown point %d", TheShape.Num);
	}
      }
    }
  | ListOfShapes tLine '{' RecursiveListOfDouble '}' tEND
    {
      for(int i = 0; i < List_Nbr($4); i++){
	double d;
	List_Read($4, i, &d);
	Shape TheShape;
	TheShape.Num = (int)d;
	Curve *c = FindCurve(TheShape.Num);
	if(c){
	  TheShape.Type = c->Typ;
	  List_Add($$, &TheShape);
	}
	else{
	  GEdge *ge = GModel::current()->getEdgeByTag(TheShape.Num);
	  if(ge){
	    TheShape.Type = MSH_SEGM_FROM_GMODEL;
	    List_Add($$, &TheShape);
	  }
	  else
	    yymsg(1, "Unknown curve %d", TheShape.Num);
	}
      }
    }
  | ListOfShapes tSurface '{' RecursiveListOfDouble '}' tEND
    {
      for(int i = 0; i < List_Nbr($4); i++){
	double d;
	List_Read($4, i, &d);
	Shape TheShape;
	TheShape.Num = (int)d;
	Surface *s = FindSurface(TheShape.Num);
	if(s){
	  TheShape.Type = s->Typ;
	  List_Add($$, &TheShape);
	}
	else{
	  GFace *gf = GModel::current()->getFaceByTag(TheShape.Num);
	  if(gf){
	    TheShape.Type = MSH_SURF_FROM_GMODEL;
	    List_Add($$, &TheShape);
	  }
	  else
	    yymsg(1, "Unknown surface %d", TheShape.Num);
	}
      }
    }
  | ListOfShapes tVolume '{' RecursiveListOfDouble '}' tEND
    {
      for(int i = 0; i < List_Nbr($4); i++){
	double d;
	List_Read($4, i, &d);
	Shape TheShape;
	TheShape.Num = (int)d;
	Volume *v = FindVolume(TheShape.Num);
	if(v){
	  TheShape.Type = v->Typ;
	  List_Add($$, &TheShape);
	}
	else{
	  GRegion *gr = GModel::current()->getRegionByTag(TheShape.Num);
	  if(gr){
	    TheShape.Type = MSH_VOLUME_FROM_GMODEL;
	    List_Add($$, &TheShape);
	  }
	  else
	    yymsg(1, "Unknown volume %d", TheShape.Num);
	}
      }
    }
;

//  D E L E T E

Delete :
    tDelete '{' ListOfShapes '}'
    {
      for(int i = 0; i < List_Nbr($3); i++){
	Shape TheShape;
	List_Read($3, i, &TheShape);
	DeleteShape(TheShape.Type, TheShape.Num);
      }
      List_Delete($3);
    }
  | tDelete tField '[' FExpr ']' tEND
    {
      GModel::current()->getFields()->delete_field((int)$4);
    }
  | tDelete tSTRING '[' FExpr ']' tEND
    {
#if !defined(HAVE_NO_POST)
      if(!strcmp($2, "View")){
	int index = (int)$4;
	if(index >= 0 && index < (int)PView::list.size())
	  delete PView::list[index];
	else
	  yymsg(0, "Unknown view %d", index);
      }
      else
	yymsg(0, "Unknown command 'Delete %s'", $2);
#endif
      Free($2);
    }
  | tDelete tSTRING tEND
    {
      if(!strcmp($2, "Meshes") || !strcmp($2, "All")){
	GModel::current()->destroy();
	GModel::current()->getGEOInternals()->destroy();
      }
      else if(!strcmp($2, "Physicals")){
	GModel::current()->getGEOInternals()->reset_physicals();
	GModel::current()->deletePhysicalGroups();
      }
      else
	yymsg(0, "Unknown command 'Delete %s'", $2);
      Free($2);
    }
  | tDelete tSTRING tSTRING tEND
    {
#if !defined(HAVE_NO_POST)
      if(!strcmp($2, "Empty") && !strcmp($3, "Views")){
	for(int i = PView::list.size() - 1; i >= 0; i--)
	  if(PView::list[i]->getData()->empty()) delete PView::list[i];
      }
      else
	yymsg(0, "Unknown command 'Delete %s %s'", $2, $3);
#endif
      Free($2); Free($3);
    }
;

//  C O L O R I F Y

Colorify :
    tColor ColorExpr '{' ListOfShapes '}'
    {
      for(int i = 0; i < List_Nbr($4); i++){
	Shape TheShape;
	List_Read($4, i, &TheShape);
	ColorShape(TheShape.Type, TheShape.Num, $2);
      }
      List_Delete($4);      
    }
;

//  V I S I B I L I T Y

Visibility :
    tShow StringExprVar tEND
    {
      for(int i = 0; i < 4; i++)
	VisibilityShape($2, i, 1);
      Free($2);
    }
  | tHide StringExprVar tEND
    {
      for(int i = 0; i < 4; i++)
	VisibilityShape($2, i, 0);
      Free($2);
    }
  | tShow '{' ListOfShapes '}'
    {
      for(int i = 0; i < List_Nbr($3); i++){
	Shape TheShape;
	List_Read($3, i, &TheShape);
	VisibilityShape(TheShape.Type, TheShape.Num, 1);
      }
      List_Delete($3);
    }
  | tHide '{' ListOfShapes '}'
    {
      for(int i = 0; i < List_Nbr($3); i++){
	Shape TheShape;
	List_Read($3, i, &TheShape);
	VisibilityShape(TheShape.Type, TheShape.Num, 0);
      }
      List_Delete($3);
    }
;

//  C O M M A N D  

Command :
    tSTRING StringExpr tEND
    {
      if(!strcmp($1, "Include")){
	char tmpstring[1024];
	FixRelativePath($2, tmpstring);
	// Warning: we *don't* close included files (to allow user
	// functions in these files). If you need to include many many
	// files and don't have functions in the files, use "Merge"
	// instead: some OSes limit the number of files a process can
	// open simultaneously. The right solution would be of course
	// to modify FunctionManager to reopen the files instead of
	// using the FILE pointer, but hey, I'm lazy...
	Msg::StatusBar(2, true, "Reading '%s'", tmpstring);
	ParseFile(tmpstring, 0, 1);
	SetBoundingBox();
	Msg::StatusBar(2, true, "Read '%s'", tmpstring);
      }
      else if(!strcmp($1, "Print")){
#if defined(HAVE_FLTK)
	// make sure we have the latest data from GEO_Internals in GModel
	// (fixes bug where we would have no geometry in the picture if
	// the print command is in the same file as the geometry)
	GModel::current()->importGEOInternals();
	char tmpstring[1024];
	FixRelativePath($2, tmpstring);
	CreateOutputFile(tmpstring, CTX.print.format);
#endif
      }
      else if(!strcmp($1, "Save")){
#if defined(HAVE_FLTK)
	GModel::current()->importGEOInternals();
	char tmpstring[1024];
	FixRelativePath($2, tmpstring);
	CreateOutputFile(tmpstring, CTX.mesh.format);
#endif
      }
      else if(!strcmp($1, "Merge") || !strcmp($1, "MergeWithBoundingBox")){
	// MergeWithBoundingBox is deprecated
	char tmpstring[1024];
	FixRelativePath($2, tmpstring);
	MergeFile(tmpstring, 1);
      }
      else if(!strcmp($1, "System"))
	SystemCall($2);
      else
	yymsg(0, "Unknown command '%s'", $1);
      Free($1); Free($2);
    } 
  | tSTRING tSTRING '[' FExpr ']' StringExprVar tEND
    {
#if !defined(HAVE_NO_POST)
      if(!strcmp($1, "Save") && !strcmp($2, "View")){
	int index = (int)$4;
	if(index >= 0 && index < (int)PView::list.size()){
	  char tmpstring[1024];
	  FixRelativePath($6, tmpstring);
	  PView::list[index]->write(tmpstring, CTX.post.file_format);
	}
	else
	  yymsg(0, "Unknown view %d", index);
      }
      else
	yymsg(0, "Unknown command '%s'", $1);
#endif
      Free($1); Free($2); Free($6);
    }
  | tSTRING tSTRING tSTRING '[' FExpr ']' tEND
    {
#if !defined(HAVE_NO_POST)
      if(!strcmp($1, "Background") && !strcmp($2, "Mesh")  && !strcmp($3, "View")){
	int index = (int)$5;
	if(index >= 0 && index < (int)PView::list.size())
	  GModel::current()->getFields()->set_background_mesh(index);
	else
	  yymsg(0, "Unknown view %d", index);
      }
      else
	yymsg(0, "Unknown command '%s'", $1);
#endif
      Free($1); Free($2); Free($3);
    }
  | tSTRING FExpr tEND
    {
      if(!strcmp($1, "Sleep")){
	SleepInSeconds($2);
      }
      else if(!strcmp($1, "Remesh")){
	Msg::Error("Surface remeshing must be reinterfaced");
      }
      else if(!strcmp($1, "Mesh")){
	int lock = CTX.threads_lock;
	CTX.threads_lock = 0;
	GModel::current()->importGEOInternals();
	GModel::current()->mesh((int)$2);
	CTX.threads_lock = lock;
      }
      else
	yymsg(0, "Unknown command '%s'", $1);
      Free($1);
    }
   | tPlugin '(' tSTRING ')' '.' tSTRING tEND
     {
#if !defined(HAVE_NO_POST)
       try {
	 GMSH_PluginManager::instance()->action($3, $6, 0);
       }
       catch(...) {
	 yymsg(0, "Unknown action '%s' or plugin '%s'", $6, $3);
       }
#endif
       Free($3); Free($6);
     }
   | tCombine tSTRING tEND
    {
#if !defined(HAVE_NO_POST)
      if(!strcmp($2, "ElementsFromAllViews"))
	PView::combine(false, 1, CTX.post.combine_remove_orig);
      else if(!strcmp($2, "ElementsFromVisibleViews"))
	PView::combine(false, 0, CTX.post.combine_remove_orig);
      else if(!strcmp($2, "ElementsByViewName"))
	PView::combine(false, 2, CTX.post.combine_remove_orig);
      else if(!strcmp($2, "TimeStepsFromAllViews"))
	PView::combine(true, 1, CTX.post.combine_remove_orig);
      else if(!strcmp($2, "TimeStepsFromVisibleViews"))
	PView::combine(true, 0, CTX.post.combine_remove_orig);
      else if(!strcmp($2, "TimeStepsByViewName"))
	PView::combine(true, 2, CTX.post.combine_remove_orig);
      else if(!strcmp($2, "Views"))
	PView::combine(false, 1, CTX.post.combine_remove_orig);
      else if(!strcmp($2, "TimeSteps"))
	PView::combine(true, 2, CTX.post.combine_remove_orig);
      else
	yymsg(0, "Unknown 'Combine' command");
#endif
      Free($2);
    } 
   | tExit tEND
    {
      exit(0);
    } 
   | tBoundingBox tEND
    {
      CTX.forced_bbox = 0;
      SetBoundingBox();
    } 
   | tBoundingBox '{' FExpr ',' FExpr ',' FExpr ',' FExpr ',' FExpr ',' FExpr '}' tEND
    {
      CTX.forced_bbox = 1;
      SetBoundingBox($3, $5, $7, $9, $11, $13);
    } 
   | tDraw tEND
    {
#if defined(HAVE_FLTK)
      Draw();
#endif
    }
;

// L O O P  

Loop :   

    tFor '(' FExpr tDOTS FExpr ')'
    {
      LoopControlVariablesTab[ImbricatedLoop][0] = $3;
      LoopControlVariablesTab[ImbricatedLoop][1] = $5;
      LoopControlVariablesTab[ImbricatedLoop][2] = 1.0;
      LoopControlVariablesNameTab[ImbricatedLoop] = NULL;
      fgetpos(gmsh_yyin, &yyposImbricatedLoopsTab[ImbricatedLoop]);
      yylinenoImbricatedLoopsTab[ImbricatedLoop] = gmsh_yylineno;
      if($3 > $5)
	skip_until("For", "EndFor");
      else
	ImbricatedLoop++;
      if(ImbricatedLoop > MAX_RECUR_LOOPS - 1){
	yymsg(0, "Reached maximum number of imbricated loops");
	ImbricatedLoop = MAX_RECUR_LOOPS - 1;
      }
    }
  | tFor '(' FExpr tDOTS FExpr tDOTS FExpr ')'
    {
      LoopControlVariablesTab[ImbricatedLoop][0] = $3;
      LoopControlVariablesTab[ImbricatedLoop][1] = $5;
      LoopControlVariablesTab[ImbricatedLoop][2] = $7;
      LoopControlVariablesNameTab[ImbricatedLoop] = NULL;
      fgetpos(gmsh_yyin, &yyposImbricatedLoopsTab[ImbricatedLoop]);
      yylinenoImbricatedLoopsTab[ImbricatedLoop] = gmsh_yylineno;
      if(($7 > 0. && $3 > $5) || ($7 < 0. && $3 < $5))
	skip_until("For", "EndFor");
      else
	ImbricatedLoop++;
      if(ImbricatedLoop > MAX_RECUR_LOOPS - 1){
	yymsg(0, "Reached maximum number of imbricated loops");
	ImbricatedLoop = MAX_RECUR_LOOPS - 1;
      }
    }
  | tFor tSTRING tIn '{' FExpr tDOTS FExpr '}' 
    {
      LoopControlVariablesTab[ImbricatedLoop][0] = $5;
      LoopControlVariablesTab[ImbricatedLoop][1] = $7;
      LoopControlVariablesTab[ImbricatedLoop][2] = 1.0;
      LoopControlVariablesNameTab[ImbricatedLoop] = $2;
      Symbol TheSymbol, *pSymbol;
      TheSymbol.Name = $2;
      if(!(pSymbol = (Symbol*)Tree_PQuery(Symbol_T, &TheSymbol))){
	TheSymbol.val = List_Create(1, 1, sizeof(double));
	List_Put(TheSymbol.val, 0, &$5);
	Tree_Add(Symbol_T, &TheSymbol);
      }
      else
	List_Write(pSymbol->val, 0, &$5);
      fgetpos(gmsh_yyin, &yyposImbricatedLoopsTab[ImbricatedLoop]);
      yylinenoImbricatedLoopsTab[ImbricatedLoop] = gmsh_yylineno;
      if($5 > $7) 
	skip_until("For", "EndFor");
      else
	ImbricatedLoop++;
      if(ImbricatedLoop > MAX_RECUR_LOOPS - 1){
	yymsg(0, "Reached maximum number of imbricated loops");
	ImbricatedLoop = MAX_RECUR_LOOPS - 1;
      }
    }
  | tFor tSTRING tIn '{' FExpr tDOTS FExpr tDOTS FExpr '}' 
    {
      LoopControlVariablesTab[ImbricatedLoop][0] = $5;
      LoopControlVariablesTab[ImbricatedLoop][1] = $7;
      LoopControlVariablesTab[ImbricatedLoop][2] = $9;
      LoopControlVariablesNameTab[ImbricatedLoop] = $2;
      Symbol TheSymbol, *pSymbol;
      TheSymbol.Name = $2;
      if(!(pSymbol = (Symbol*)Tree_PQuery(Symbol_T, &TheSymbol))){
	TheSymbol.val = List_Create(1, 1, sizeof(double));
	List_Put(TheSymbol.val, 0, &$5);
	Tree_Add(Symbol_T, &TheSymbol);
      }
      else
	List_Write(pSymbol->val, 0, &$5);
      fgetpos(gmsh_yyin, &yyposImbricatedLoopsTab[ImbricatedLoop]);
      yylinenoImbricatedLoopsTab[ImbricatedLoop] = gmsh_yylineno;
      if(($9 > 0. && $5 > $7) || ($9 < 0. && $5 < $7))
	skip_until("For", "EndFor");
      else
	ImbricatedLoop++;
      if(ImbricatedLoop > MAX_RECUR_LOOPS - 1){
	yymsg(0, "Reached maximum number of imbricated loops");
	ImbricatedLoop = MAX_RECUR_LOOPS - 1;
      }
    }
  | tEndFor 
    {
      if(ImbricatedLoop <= 0){
	yymsg(0, "Invalid For/EndFor loop");
	ImbricatedLoop = 0;
      }
      else{
	double x0 = LoopControlVariablesTab[ImbricatedLoop - 1][0];
	double x1 = LoopControlVariablesTab[ImbricatedLoop - 1][1];
	double step = LoopControlVariablesTab[ImbricatedLoop - 1][2];
	int do_next = (step > 0.) ? (x0 + step <= x1) : (x0 + step >= x1);
	if(do_next){
	  LoopControlVariablesTab[ImbricatedLoop - 1][0] +=
	    LoopControlVariablesTab[ImbricatedLoop - 1][2];
	  if(LoopControlVariablesNameTab[ImbricatedLoop - 1]){
	    Symbol TheSymbol, *pSymbol;
	    TheSymbol.Name = LoopControlVariablesNameTab[ImbricatedLoop - 1];
	    if(!(pSymbol = (Symbol*)Tree_PQuery(Symbol_T, &TheSymbol)))
	      yymsg(0, "Unknown loop variable");
	    else
	      *(double*)List_Pointer_Fast(pSymbol->val, 0) += 
		LoopControlVariablesTab[ImbricatedLoop - 1][2];
	  }
	  fsetpos(gmsh_yyin, &yyposImbricatedLoopsTab[ImbricatedLoop - 1]);
	  gmsh_yylineno = yylinenoImbricatedLoopsTab[ImbricatedLoop - 1];
	}
	else
	  ImbricatedLoop--;
      }
    }
  | tFunction tSTRING
    {
      if(!FunctionManager::Instance()->createFunction($2, gmsh_yyin, gmsh_yyname,
						      gmsh_yylineno))
	yymsg(0, "Redefinition of function %s", $2);
      skip_until(NULL, "Return");
      //FIXME: wee leak $2
    }
  | tReturn
    {
      if(!FunctionManager::Instance()->leaveFunction(&gmsh_yyin, gmsh_yyname,
						     gmsh_yylineno))
	yymsg(0, "Error while exiting function");
    } 
  | tCall tSTRING tEND
    {
      if(!FunctionManager::Instance()->enterFunction($2, &gmsh_yyin, gmsh_yyname,
						     gmsh_yylineno))
	yymsg(0, "Unknown function %s", $2);
      //FIXME: wee leak $2
    } 
  | tIf '(' FExpr ')'
    {
      if(!$3) skip_until("If", "EndIf");
    }
  | tEndIf
    {
    }
;


//  E X T R U D E 

Extrude :
    tExtrude VExpr '{' ListOfShapes '}'
    {
      $$ = List_Create(2, 1, sizeof(Shape));
      ExtrudeShapes(TRANSLATE, $4, 
		    $2[0], $2[1], $2[2], 0., 0., 0., 0., 0., 0., 0.,
		    NULL, $$);
      List_Delete($4);
    }
  | tExtrude '{' VExpr ',' VExpr ',' FExpr '}' '{' ListOfShapes '}'
    {
      $$ = List_Create(2, 1, sizeof(Shape));
      ExtrudeShapes(ROTATE, $10, 
		    0., 0., 0., $3[0], $3[1], $3[2], $5[0], $5[1], $5[2], $7,
		    NULL, $$);
      List_Delete($10);
    }
  | tExtrude '{' VExpr ',' VExpr ',' VExpr ',' FExpr '}' '{' ListOfShapes '}'
    {
      $$ = List_Create(2, 1, sizeof(Shape));
      ExtrudeShapes(TRANSLATE_ROTATE, $12, 
		    $3[0], $3[1], $3[2], $5[0], $5[1], $5[2], $7[0], $7[1], $7[2], $9,
		    NULL, $$);
      List_Delete($12);
    }
  | tExtrude VExpr '{' ListOfShapes 
    {
      extr.mesh.ExtrudeMesh = extr.mesh.Recombine = false;
    }
                       ExtrudeParameters '}'
    {
      $$ = List_Create(2, 1, sizeof(Shape));
      ExtrudeShapes(TRANSLATE, $4, 
		    $2[0], $2[1], $2[2], 0., 0., 0., 0., 0., 0., 0.,
		    &extr, $$);
      List_Delete($4);
    }
  | tExtrude '{' VExpr ',' VExpr ',' FExpr '}' '{' ListOfShapes 
    {
      extr.mesh.ExtrudeMesh = extr.mesh.Recombine = false;
    }
                                                   ExtrudeParameters '}'
    {
      $$ = List_Create(2, 1, sizeof(Shape));
      ExtrudeShapes(ROTATE, $10, 
		    0., 0., 0., $3[0], $3[1], $3[2], $5[0], $5[1], $5[2], $7,
		    &extr, $$);
      List_Delete($10);
    }
  | tExtrude '{' VExpr ',' VExpr ',' VExpr ',' FExpr '}' '{' ListOfShapes
    {
      extr.mesh.ExtrudeMesh = extr.mesh.Recombine = false;
    }
                                                             ExtrudeParameters '}'
    {
      $$ = List_Create(2, 1, sizeof(Shape));
      ExtrudeShapes(TRANSLATE_ROTATE, $12, 
		    $3[0], $3[1], $3[2], $5[0], $5[1], $5[2], $7[0], $7[1], $7[2], $9,
		    &extr, $$);
      List_Delete($12);
    }
  | tExtrude '{' ListOfShapes 
    {
      extr.mesh.ExtrudeMesh = extr.mesh.Recombine = false;
    }
                       ExtrudeParameters '}'
    {
      $$ = List_Create(2, 1, sizeof(Shape));
      ExtrudeShapes(BOUNDARY_LAYER, $3, 0., 0., 0., 0., 0., 0., 0., 0., 0., 0.,
		    &extr, $$);
      List_Delete($3);
    }
  | tExtrude tSTRING '[' FExpr ']' '{' ListOfShapes 
    {
      extr.mesh.ExtrudeMesh = extr.mesh.Recombine = false;
    }
                       ExtrudeParameters '}'
    {
      $$ = List_Create(2, 1, sizeof(Shape));
      extr.mesh.ViewIndex = (int)$4;
      ExtrudeShapes(BOUNDARY_LAYER, $7, 0., 0., 0., 0., 0., 0., 0., 0., 0., 0.,
		    &extr, $$);
      extr.mesh.ViewIndex = -1;
      Free($2);
      List_Delete($7);
    }

  // Deprecated extrude commands (for backward compatibility)
  | tExtrude tPoint '{' FExpr ',' VExpr '}' tEND
    {
      $$ = List_Create(2, 1, sizeof(Shape));
      ExtrudeShape(TRANSLATE, MSH_POINT, (int)$4, 
		   $6[0], $6[1], $6[2], 0., 0., 0., 0., 0., 0., 0.,
		   NULL, $$);
    }
  | tExtrude tLine '{' FExpr ',' VExpr '}' tEND
    {
      $$ = List_Create(2, 1, sizeof(Shape));
      ExtrudeShape(TRANSLATE, MSH_SEGM_LINE, (int)$4, 
		   $6[0], $6[1], $6[2], 0., 0., 0., 0., 0., 0., 0.,
		   NULL, $$);
    }
  | tExtrude tSurface '{' FExpr ',' VExpr '}' tEND
    {
      $$ = List_Create(2, 1, sizeof(Shape));
      ExtrudeShape(TRANSLATE, MSH_SURF_PLAN, (int)$4, 
		   $6[0], $6[1], $6[2], 0., 0., 0., 0., 0., 0., 0.,
		   NULL, $$);
    }
  | tExtrude tPoint '{' FExpr ',' VExpr ',' VExpr ',' FExpr '}'  tEND
    {
      $$ = List_Create(2, 1, sizeof(Shape));
      ExtrudeShape(ROTATE, MSH_POINT, (int)$4, 
		   0., 0., 0., $6[0], $6[1], $6[2], $8[0], $8[1], $8[2], $10,
		   NULL, $$);
    }
  | tExtrude tLine '{' FExpr ',' VExpr ',' VExpr ',' FExpr '}' tEND
    {
      $$ = List_Create(2, 1, sizeof(Shape));
      ExtrudeShape(ROTATE, MSH_SEGM_LINE, (int)$4, 
		   0., 0., 0., $6[0], $6[1], $6[2], $8[0], $8[1], $8[2], $10,
		   NULL, $$);
    }
  | tExtrude tSurface '{' FExpr ',' VExpr ',' VExpr ',' FExpr '}' tEND
    {
      $$ = List_Create(2, 1, sizeof(Shape));
      ExtrudeShape(ROTATE, MSH_SURF_PLAN, (int)$4, 
		   0., 0., 0., $6[0], $6[1], $6[2], $8[0], $8[1], $8[2], $10,
		   NULL, $$);
    }
  | tExtrude tPoint '{' FExpr ',' VExpr ',' VExpr ',' VExpr ',' FExpr'}'  tEND
    {
      $$ = List_Create(2, 1, sizeof(Shape));
      ExtrudeShape(TRANSLATE_ROTATE, MSH_POINT, (int)$4, 
		   $6[0], $6[1], $6[2], $8[0], $8[1], $8[2], $10[0], $10[1], $10[2], $12,
		   NULL, $$);
    }
  | tExtrude tLine '{' FExpr ',' VExpr ',' VExpr ',' VExpr ',' FExpr '}' tEND
    {
      $$ = List_Create(2, 1, sizeof(Shape));
      ExtrudeShape(TRANSLATE_ROTATE, MSH_SEGM_LINE, (int)$4, 
		   $6[0], $6[1], $6[2], $8[0], $8[1], $8[2], $10[0], $10[1], $10[2], $12,
		   NULL, $$);
    }
  | tExtrude tSurface '{' FExpr ',' VExpr ',' VExpr ',' VExpr ',' FExpr '}' tEND
    {
      $$ = List_Create(2, 1, sizeof(Shape));
      ExtrudeShape(TRANSLATE_ROTATE, MSH_SURF_PLAN, (int)$4, 
		   $6[0], $6[1], $6[2], $8[0], $8[1], $8[2], $10[0], $10[1], $10[2], $12,
		   NULL, $$);
    }
  | tExtrude tPoint '{' FExpr ',' VExpr '}' 
    {
      extr.mesh.ExtrudeMesh = extr.mesh.Recombine = false;
    }
                    '{' ExtrudeParameters '}' tEND
    {
      $$ = List_Create(2, 1, sizeof(Shape));
      ExtrudeShape(TRANSLATE, MSH_POINT, (int)$4, 
		   $6[0], $6[1], $6[2], 0., 0., 0., 0., 0., 0., 0.,
		   &extr, $$);
    }
  | tExtrude tLine '{' FExpr ',' VExpr '}'
    {
      extr.mesh.ExtrudeMesh = extr.mesh.Recombine = false;
    }
                   '{' ExtrudeParameters '}' tEND
    {
      $$ = List_Create(2, 1, sizeof(Shape));
      ExtrudeShape(TRANSLATE, MSH_SEGM_LINE, (int)$4, 
		   $6[0], $6[1], $6[2], 0., 0., 0., 0., 0., 0., 0.,
		   &extr, $$);
    }
  | tExtrude tSurface '{' FExpr ',' VExpr '}' 
    {
      extr.mesh.ExtrudeMesh = extr.mesh.Recombine = false;
    }
                      '{' ExtrudeParameters '}' tEND
    {
      $$ = List_Create(2, 1, sizeof(Shape));
      ExtrudeShape(TRANSLATE, MSH_SURF_PLAN, (int)$4, 
		   $6[0], $6[1], $6[2], 0., 0., 0., 0., 0., 0., 0.,
		   &extr, $$);
    }
  | tExtrude tPoint '{' FExpr ',' VExpr ',' VExpr ',' FExpr '}'
    {
      extr.mesh.ExtrudeMesh = extr.mesh.Recombine = false;
    }
                    '{' ExtrudeParameters '}' tEND
    {
      $$ = List_Create(2, 1, sizeof(Shape));
      ExtrudeShape(ROTATE, MSH_POINT, (int)$4, 
		   0., 0., 0., $6[0], $6[1], $6[2], $8[0], $8[1], $8[2], $10,
		   &extr, $$);
    }
  | tExtrude tLine '{' FExpr ',' VExpr ',' VExpr ',' FExpr '}'
    {
      extr.mesh.ExtrudeMesh = extr.mesh.Recombine = false;
    }
                   '{' ExtrudeParameters '}' tEND
    {
      $$ = List_Create(2, 1, sizeof(Shape));
      ExtrudeShape(ROTATE, MSH_SEGM_LINE, (int)$4, 
		   0., 0., 0., $6[0], $6[1], $6[2], $8[0], $8[1], $8[2], $10,
		   &extr, $$);
    }
  | tExtrude tSurface '{' FExpr ',' VExpr ',' VExpr ',' FExpr '}'
    {
      extr.mesh.ExtrudeMesh = extr.mesh.Recombine = false;
    }
                      '{' ExtrudeParameters '}' tEND
    {
      $$ = List_Create(2, 1, sizeof(Shape));
      ExtrudeShape(ROTATE, MSH_SURF_PLAN, (int)$4, 
		   0., 0., 0., $6[0], $6[1], $6[2], $8[0], $8[1], $8[2], $10,
		   &extr, $$);
    }
  | tExtrude tPoint '{' FExpr ',' VExpr ',' VExpr ',' VExpr ',' FExpr'}' 
    {
      extr.mesh.ExtrudeMesh = extr.mesh.Recombine = false;
    }
                    '{' ExtrudeParameters '}' tEND
    {
      $$ = List_Create(2, 1, sizeof(Shape));
      ExtrudeShape(TRANSLATE_ROTATE, MSH_POINT, (int)$4, 
		   $6[0], $6[1], $6[2], $8[0], $8[1], $8[2], $10[0], $10[1], $10[2], $12,
		   &extr, $$);
    }
  | tExtrude tLine '{' FExpr ',' VExpr ',' VExpr ',' VExpr ',' FExpr '}' 
    {
      extr.mesh.ExtrudeMesh = extr.mesh.Recombine = false;
    }
                   '{' ExtrudeParameters '}' tEND
    {
      $$ = List_Create(2, 1, sizeof(Shape));
      ExtrudeShape(TRANSLATE_ROTATE, MSH_SEGM_LINE, (int)$4, 
		   $6[0], $6[1], $6[2], $8[0], $8[1], $8[2], $10[0], $10[1], $10[2], $12,
		   &extr, $$);
    }
  | tExtrude tSurface '{' FExpr ',' VExpr ',' VExpr ',' VExpr ',' FExpr '}' 
    {
      extr.mesh.ExtrudeMesh = extr.mesh.Recombine = false;
    }
                      '{' ExtrudeParameters '}' tEND
    {
      $$ = List_Create(2, 1, sizeof(Shape));
      ExtrudeShape(TRANSLATE_ROTATE, MSH_SURF_PLAN, (int)$4, 
		   $6[0], $6[1], $6[2], $8[0], $8[1], $8[2], $10[0], $10[1], $10[2], $12,
		   &extr, $$);
    }
  // End of deprecated Extrude commands
;

ExtrudeParameters :
    ExtrudeParameter
    {
    }
  | ExtrudeParameters ExtrudeParameter
    {
    }
;

ExtrudeParameter :
    tLayers '{' FExpr '}' tEND
    {
      extr.mesh.ExtrudeMesh = true;
      extr.mesh.NbLayer = 1;
      extr.mesh.NbElmLayer.clear();
      extr.mesh.hLayer.clear();
      extr.mesh.NbElmLayer.push_back((int)fabs($3));
      extr.mesh.hLayer.push_back(1.);
    }
  | tLayers '{' ListOfDouble ',' ListOfDouble '}' tEND
    {
      extr.mesh.ExtrudeMesh = true;
      extr.mesh.NbLayer = List_Nbr($3);
      if(List_Nbr($3) == List_Nbr($5)){
	extr.mesh.NbElmLayer.clear();
	extr.mesh.hLayer.clear();
	for(int i = 0; i < List_Nbr($3); i++){
	  double d;
	  List_Read($3, i, &d);
	  extr.mesh.NbElmLayer.push_back((d > 0) ? (int)d : 1);
	  List_Read($5, i, &d);
	  extr.mesh.hLayer.push_back(d);
	}
      }
      else
	yymsg(0, "Wrong layer definition {%d, %d}", List_Nbr($3), List_Nbr($5));
      List_Delete($3);
      List_Delete($5);
    }
  | tLayers '{' ListOfDouble ',' ListOfDouble ',' ListOfDouble '}' tEND
    {
      yymsg(0, "Explicit region numbers in layers are deprecated");
      extr.mesh.ExtrudeMesh = true;
      extr.mesh.NbLayer = List_Nbr($3);
      if(List_Nbr($3) == List_Nbr($5) && List_Nbr($3) == List_Nbr($7)){
	extr.mesh.NbElmLayer.clear();
	extr.mesh.hLayer.clear();
	for(int i = 0; i < List_Nbr($3); i++){
	  double d;
	  List_Read($3, i, &d);
	  extr.mesh.NbElmLayer.push_back((d > 0) ? (int)d : 1);
	  List_Read($7, i, &d);
	  extr.mesh.hLayer.push_back(d);
	}
      }
      else
	yymsg(0, "Wrong layer definition {%d, %d, %d}", List_Nbr($3), 
	      List_Nbr($5), List_Nbr($7));
      List_Delete($3);
      List_Delete($5);
      List_Delete($7);
    }
  | tRecombine tEND
    {
      extr.mesh.Recombine = true;
    }
  | tHole '(' FExpr ')' tAFFECT ListOfDouble tUsing FExpr tEND
    {
      int num = (int)$3;
      if(FindSurface(num)){
	yymsg(0, "Surface %d already exists", num);
      }
      else{
	Surface *s = Create_Surface(num, MSH_SURF_DISCRETE);
	Tree_Add(GModel::current()->getGEOInternals()->Surfaces, &s);
	extr.mesh.Holes[num].first = $8;
	extr.mesh.Holes[num].second.clear();
	for(int i = 0; i < List_Nbr($6); i++){
	  double d;
	  List_Read($6, i, &d);
	  extr.mesh.Holes[num].second.push_back((int)d);
	}
      }
      List_Delete($6);
    }
;

//  T R A N S F I N I T E

Transfinite : 
    tTransfinite tLine ListOfDouble tAFFECT FExpr tEND
    {
      for(int i = 0; i < List_Nbr($3); i++){
	double d;
	List_Read($3, i, &d);
	int j = (int)fabs(d);
        Curve *c = FindCurve(j);
	if(!c)
	  yymsg(1, "Unknown curve %d", j);
	else{
	  c->Method = MESH_TRANSFINITE;
	  c->nbPointsTransfinite = ($5 > 2) ? (int)$5 : 2;
	  c->typeTransfinite = sign(d);
	  c->coeffTransfinite = 1.0;
	}
      }
      List_Delete($3);
    }
  | tTransfinite tLine ListOfDouble tAFFECT FExpr tUsing tProgression FExpr tEND
    {
      for(int i = 0; i < List_Nbr($3); i++){
	double d;
	List_Read($3, i, &d);
	int j = (int)fabs(d);
        Curve *c = FindCurve(j);
	if(!c)
	  yymsg(1, "Unknown curve %d", j);
	else{
	  c->Method = MESH_TRANSFINITE;
	  c->nbPointsTransfinite = ($5 > 2) ? (int)$5 : 2;
	  c->typeTransfinite = sign(d); // Progresion : code 1 ou -1
	  c->coeffTransfinite = fabs($8);
	}
      }
      List_Delete($3);
    }
  | tTransfinite tLine ListOfDouble tAFFECT FExpr tUsing tBump FExpr tEND
    {
      for(int i = 0; i < List_Nbr($3); i++){
	double d;
	List_Read($3, i, &d);
	int j = (int)fabs(d);
        Curve *c = FindCurve(j);
	if(!c)
	  yymsg(1, "Unknown curve %d", j);
	else{
	  c->Method = MESH_TRANSFINITE;
	  c->nbPointsTransfinite = ($5 > 2) ? (int)$5 : 2;
	  c->typeTransfinite = 2 * sign(d); // Bump : code 2 ou -2
	  c->coeffTransfinite = fabs($8);
	}
      }
      List_Delete($3);
    }
  | tTransfinite tSurface '{' FExpr '}' tAFFECT ListOfDouble tEND
    {
      Surface *s = FindSurface((int)$4);
      if(!s)
	yymsg(1, "Unknown surface %d", (int)$4);
      else{
	s->Method = MESH_TRANSFINITE;
	s->Recombine_Dir = -1;
	int k = List_Nbr($7);
	if(k != 3 && k != 4){
	  yymsg(0, "Wrong definition of Transfinite Surface %d: "
		"%d points instead of 3 or 4" , (int)$4, k);
	}
	else{
	  List_Reset(s->TrsfPoints);
	  for(int i = 0; i < k; i++){
	    double d;
	    List_Read($7, i, &d);
	    int j = (int)fabs(d);
	    Vertex *v = FindPoint(j);
	    if(!v)
	      yymsg(1, "Unknown point %d", j);
	    else
	      List_Add(s->TrsfPoints, &v);
	  }
	}
      }
      List_Delete($7);
    }
  | tTransfinite tSurface '{' FExpr '}' tAFFECT ListOfDouble tSTRING tEND
    {
      Surface *s = FindSurface((int)$4);
      if(!s)
	yymsg(1, "Unknown surface %d", (int)$4);
      else{
	s->Method = MESH_TRANSFINITE;
	int k = List_Nbr($7);
	if(k != 3 && k != 4){
	  yymsg(0, "Wrong definition of Transfinite Surface %d: "
		"%d points instead of 3 or 4" , (int)$4, k);
	}
	else{
	  List_Reset(s->TrsfPoints);
	  if (!strcmp($8, "Right"))
	    s->Recombine_Dir = 1;
	  else if (!strcmp($8, "Left"))
	    s->Recombine_Dir = -1;
	  else
	    s->Recombine_Dir = 0;
	  for(int i = 0; i < k; i++){
	    double d;
	    List_Read($7, i, &d);
	    int j = (int)fabs(d);
	    Vertex *v = FindPoint(j);
	    if(!v)
	      yymsg(1, "Unknown point %d", j);
	    else
	      List_Add(s->TrsfPoints, &v);
	  }
	}
      }
      List_Delete($7);
      Free($8);
    }
  | tElliptic tSurface '{' FExpr '}' tAFFECT ListOfDouble tEND
    {
      yymsg(1, "Elliptic Surface is deprecated: use Transfinite instead (with smoothing)");
      List_Delete($7);
    }
  | tTransfinite tVolume '{' FExpr '}' tAFFECT ListOfDouble tEND
    {
      Volume *v = FindVolume((int)$4);
      if(!v)
	yymsg(1, "Unknown volume %d", (int)$4);
      else{
	v->Method = MESH_TRANSFINITE;
	int k = List_Nbr($7);
	if(k != 6 && k != 8)
	  yymsg(0, "Wrong definition of Transfinite Volume %d: "
		"%d points instead of 6 or 8" , (int)$4, k);
	else{
	  List_Reset(v->TrsfPoints);
	  for(int i = 0; i < k; i++){
	    double d;
	    List_Read($7, i, &d);
	    int j = (int)fabs(d);
	    Vertex *vert = FindPoint(j);
	    if(!vert)
	      yymsg(1, "Unknown point %d", j);
	    else
	      List_Add(v->TrsfPoints, &vert);
	  }
	}
      }
      List_Delete($7);
    }
  | tRecombine tSurface ListOfDouble tAFFECT FExpr tEND
    {
      for(int i = 0; i < List_Nbr($3); i++){
	double d;
	List_Read($3, i, &d);
	int j = (int)d;
	Surface *s = FindSurface(j);
	if(s){
	  s->Recombine = 1;
	  s->RecombineAngle = $5;
	}
      }
      List_Delete($3);
    }
  | tRecombine tSurface ListOfDouble tEND
    {
      for(int i = 0; i < List_Nbr($3); i++){
	double d;
	List_Read($3, i, &d);
	int j = (int)d;
        Surface *s = FindSurface(j);
	if(s){
	  s->Recombine = 1;
        }
      }
      List_Delete($3);
    }
  | tSmoother tSurface ListOfDouble tAFFECT FExpr tEND
    {
      for(int i = 0; i < List_Nbr($3); i++){
	double d;
	List_Read($3, i, &d);
	int j = (int)d;
	Surface *s = FindSurface(j);
	if(s) s->TransfiniteSmoothing = $5;
      }
      List_Delete($3);
    }
;

//  E M B E D D I N G  C U R V E S   A N D  P O I N T S   I N T O   S U R F A C E S  
//    A N D   V O L U M E S

Embedding : 
    tPoint '{' RecursiveListOfDouble '}' tIn tSurface '{' FExpr '}' tEND
    { 
      Surface *s = FindSurface((int)$8);
      if(s)
	setSurfaceEmbeddedPoints(s, $3);
    }
  | tLine '{' RecursiveListOfDouble '}' tIn tSurface '{' FExpr '}' tEND
    {
      Surface *s = FindSurface((int)$8);
      if(s)
	setSurfaceEmbeddedCurves(s, $3);
    }
  | tLine '{' RecursiveListOfDouble '}' tIn tVolume '{' FExpr '}' tEND
    {
    }
  | tSurface '{' RecursiveListOfDouble '}' tIn tVolume '{' FExpr '}' tEND
    {
    }
;


//  C O H E R E N C E

Coherence : 
    tCoherence tEND
    { 
      ReplaceAllDuplicates();
    }
;


//  G E N E R A L

FExpr :
    FExpr_Single                     { $$ = $1;           }
  | '(' FExpr ')'                    { $$ = $2;           }
  | '-' FExpr %prec UNARYPREC        { $$ = -$2;          }
  | '+' FExpr %prec UNARYPREC        { $$ = $2;           }
  | '!' FExpr                        { $$ = !$2;          }
  | FExpr '-' FExpr                  { $$ = $1 - $3;      }
  | FExpr '+' FExpr                  { $$ = $1 + $3;      }
  | FExpr '*' FExpr                  { $$ = $1 * $3;      }
  | FExpr '/' FExpr
    { 
      if(!$3)
	yymsg(0, "Division by zero in '%g / %g'", $1, $3);
      else
	$$ = $1 / $3;     
    }
  | FExpr '%' FExpr                  { $$ = (int)$1 % (int)$3;  }
  | FExpr '^' FExpr                  { $$ = pow($1, $3);  }
  | FExpr '<' FExpr                  { $$ = $1 < $3;      }
  | FExpr '>' FExpr                  { $$ = $1 > $3;      }
  | FExpr tLESSOREQUAL FExpr         { $$ = $1 <= $3;     }
  | FExpr tGREATEROREQUAL FExpr      { $$ = $1 >= $3;     }
  | FExpr tEQUAL FExpr               { $$ = $1 == $3;     }
  | FExpr tNOTEQUAL FExpr            { $$ = $1 != $3;     }
  | FExpr tAND FExpr                 { $$ = $1 && $3;     }
  | FExpr tOR FExpr                  { $$ = $1 || $3;     }
  | FExpr '?' FExpr tDOTS FExpr      { $$ = $1 ? $3 : $5; }
  | tExp    '(' FExpr ')'            { $$ = exp($3);      }
  | tLog    '(' FExpr ')'            { $$ = log($3);      }
  | tLog10  '(' FExpr ')'            { $$ = log10($3);    }
  | tSqrt   '(' FExpr ')'            { $$ = sqrt($3);     }
  | tSin    '(' FExpr ')'            { $$ = sin($3);      }
  | tAsin   '(' FExpr ')'            { $$ = asin($3);     }
  | tCos    '(' FExpr ')'            { $$ = cos($3);      }
  | tAcos   '(' FExpr ')'            { $$ = acos($3);     }
  | tTan    '(' FExpr ')'            { $$ = tan($3);      }
  | tAtan   '(' FExpr ')'            { $$ = atan($3);     }
  | tAtan2  '(' FExpr ',' FExpr ')'  { $$ = atan2($3, $5);}
  | tSinh   '(' FExpr ')'            { $$ = sinh($3);     }
  | tCosh   '(' FExpr ')'            { $$ = cosh($3);     }
  | tTanh   '(' FExpr ')'            { $$ = tanh($3);     }
  | tFabs   '(' FExpr ')'            { $$ = fabs($3);     }
  | tFloor  '(' FExpr ')'            { $$ = floor($3);    }
  | tCeil   '(' FExpr ')'            { $$ = ceil($3);     }
  | tFmod   '(' FExpr ',' FExpr ')'  { $$ = fmod($3, $5); }
  | tModulo '(' FExpr ',' FExpr ')'  { $$ = fmod($3, $5); }
  | tHypot  '(' FExpr ',' FExpr ')'  { $$ = sqrt($3 * $3 + $5 * $5); }
  | tRand   '(' FExpr ')'            { $$ = $3 * (double)rand() / (double)RAND_MAX; }
  // The following is for GetDP compatibility
  | tExp    '[' FExpr ']'            { $$ = exp($3);      }
  | tLog    '[' FExpr ']'            { $$ = log($3);      }
  | tLog10  '[' FExpr ']'            { $$ = log10($3);    }
  | tSqrt   '[' FExpr ']'            { $$ = sqrt($3);     }
  | tSin    '[' FExpr ']'            { $$ = sin($3);      }
  | tAsin   '[' FExpr ']'            { $$ = asin($3);     }
  | tCos    '[' FExpr ']'            { $$ = cos($3);      }
  | tAcos   '[' FExpr ']'            { $$ = acos($3);     }
  | tTan    '[' FExpr ']'            { $$ = tan($3);      }
  | tAtan   '[' FExpr ']'            { $$ = atan($3);     }
  | tAtan2  '[' FExpr ',' FExpr ']'  { $$ = atan2($3, $5);}
  | tSinh   '[' FExpr ']'            { $$ = sinh($3);     }
  | tCosh   '[' FExpr ']'            { $$ = cosh($3);     }
  | tTanh   '[' FExpr ']'            { $$ = tanh($3);     }
  | tFabs   '[' FExpr ']'            { $$ = fabs($3);     }
  | tFloor  '[' FExpr ']'            { $$ = floor($3);    }
  | tCeil   '[' FExpr ']'            { $$ = ceil($3);     }
  | tFmod   '[' FExpr ',' FExpr ']'  { $$ = fmod($3, $5); }
  | tModulo '[' FExpr ',' FExpr ']'  { $$ = fmod($3, $5); }
  | tHypot  '[' FExpr ',' FExpr ']'  { $$ = sqrt($3 * $3 + $5 * $5); }
  | tRand   '[' FExpr ']'            { $$ = $3 * (double)rand() / (double)RAND_MAX; }
;

// FIXME: add +=, -=, *= et /=

FExpr_Single :

  // Constants

    tDOUBLE   { $$ = $1; }
  | tPi       { $$ = 3.141592653589793; }
  | tMPI_Rank { $$ = Msg::GetCommRank(); }
  | tMPI_Size { $$ = Msg::GetCommSize(); }
  | tGMSH_MAJOR_VERSION { $$ = Get_GmshMajorVersion(); }
  | tGMSH_MINOR_VERSION { $$ = Get_GmshMinorVersion(); }
  | tGMSH_PATCH_VERSION { $$ = Get_GmshPatchVersion(); }

  // Variables

  | tSTRING
    {
      Symbol TheSymbol, *pSymbol;
      TheSymbol.Name = $1;
      if(!(pSymbol = (Symbol*)Tree_PQuery(Symbol_T, &TheSymbol))) {
	yymsg(0, "Unknown variable '%s'", $1);
	$$ = 0.;
      }
      else
	$$ = *(double*)List_Pointer_Fast(pSymbol->val, 0);
      Free($1);
    }
  // This is for GetDP compatibility (we should generalize it so
  // that we can create variables with this syntax, use them
  // recursively, etc., but I don't have time to do it now)
  | tSTRING '~' '{' FExpr '}'
    {
      char tmpstring[1024];
      sprintf(tmpstring, "%s_%d", $1, (int)$4) ;
      Symbol TheSymbol, *pSymbol;
      TheSymbol.Name = tmpstring;
      if(!(pSymbol = (Symbol*)Tree_PQuery(Symbol_T, &TheSymbol))) {
	yymsg(0, "Unknown variable '%s'", tmpstring);
	$$ = 0.;
      }
      else
	$$ = *(double*)List_Pointer_Fast(pSymbol->val, 0);
      Free($1);
    }
  | tSTRING '[' FExpr ']'
    {
      Symbol TheSymbol, *pSymbol;
      TheSymbol.Name = $1;
      if(!(pSymbol = (Symbol*)Tree_PQuery(Symbol_T, &TheSymbol))) {
	yymsg(0, "Unknown variable '%s'", $1);
	$$ = 0.;
      }
      else{
	double *pd;
	if((pd = (double*)List_Pointer_Test(pSymbol->val, (int)$3)))
	  $$ = *pd;
	else{
	  yymsg(0, "Uninitialized variable '%s[%d]'", $1, (int)$3);
	  $$ = 0.;
	}
      }
      Free($1);
    }
  | '#' tSTRING '[' ']'
    {
      Symbol TheSymbol, *pSymbol;
      TheSymbol.Name = $2;
      if(!(pSymbol = (Symbol*)Tree_PQuery(Symbol_T, &TheSymbol))) {
	yymsg(0, "Unknown variable '%s'", $2);
	$$ = 0.;
      }
      else
	$$ = List_Nbr(pSymbol->val);
      Free($2);
    }
  | tSTRING NumericIncrement
    {
      Symbol TheSymbol, *pSymbol;
      TheSymbol.Name = $1;
      if(!(pSymbol = (Symbol*)Tree_PQuery(Symbol_T, &TheSymbol))) {
	yymsg(0, "Unknown variable '%s'", $1);
	$$ = 0.;
      }
      else
	$$ = (*(double*)List_Pointer_Fast(pSymbol->val, 0) += $2);
      Free($1);
    }
  | tSTRING '[' FExpr ']' NumericIncrement
    {
      Symbol TheSymbol, *pSymbol;
      TheSymbol.Name = $1;
      if(!(pSymbol = (Symbol*)Tree_PQuery(Symbol_T, &TheSymbol))) {
	yymsg(0, "Unknown variable '%s'", $1);
	$$ = 0.;
      }
      else{
	double *pd;
	if((pd = (double*)List_Pointer_Test(pSymbol->val, (int)$3)))
	  $$ = (*pd += $5);
	else{
	  yymsg(0, "Uninitialized variable '%s[%d]'", $1, (int)$3);
	  $$ = 0.;
	}
      }
      Free($1);
    }

  // Option Strings

  | tSTRING '.' tSTRING 
    {
      NumberOption(GMSH_GET, $1, 0, $3, $$);
      Free($1); Free($3);
    }
  | tSTRING '[' FExpr ']' '.' tSTRING 
    {
      NumberOption(GMSH_GET, $1, (int)$3, $6, $$);
      Free($1); Free($6);
    }
  | tSTRING '.' tSTRING NumericIncrement
    {
      double d = 0.;
      if(NumberOption(GMSH_GET, $1, 0, $3, d)){
	d += $4;
	NumberOption(GMSH_SET|GMSH_GUI, $1, 0, $3, d);
	$$ = d;
      }
      Free($1); Free($3);
    }
  | tSTRING '[' FExpr ']' '.' tSTRING NumericIncrement
    {
      double d = 0.;
      if(NumberOption(GMSH_GET, $1, (int)$3, $6, d)){
	d += $7;
	NumberOption(GMSH_SET|GMSH_GUI, $1, (int)$3, $6, d);
	$$ = d;
      }
      Free($1); Free($6);
    }
  | tGetValue '(' tBIGSTR ',' FExpr ')'
    { 
      $$ = Msg::GetValue($3, $5);
      Free($3);
    }
;

VExpr :
    VExpr_Single
    {
      memcpy($$, $1, 5*sizeof(double));
    }
  | '-' VExpr %prec UNARYPREC
    {
      for(int i = 0; i < 5; i++) $$[i] = -$2[i];
    }
  | '+' VExpr %prec UNARYPREC
    { 
      for(int i = 0; i < 5; i++) $$[i] = $2[i];
    }
  | VExpr '-' VExpr
    { 
      for(int i = 0; i < 5; i++) $$[i] = $1[i] - $3[i];
    }
  | VExpr '+' VExpr
    {
      for(int i = 0; i < 5; i++) $$[i] = $1[i] + $3[i];
    }
;

VExpr_Single :
    '{' FExpr ',' FExpr ',' FExpr ',' FExpr ',' FExpr  '}'
    { 
      $$[0] = $2;  $$[1] = $4;  $$[2] = $6;  $$[3] = $8; $$[4] = $10;
    }
  | '{' FExpr ',' FExpr ',' FExpr ',' FExpr '}'
    { 
      $$[0] = $2;  $$[1] = $4;  $$[2] = $6;  $$[3] = $8; $$[4] = 1.0;
    }
  | '{' FExpr ',' FExpr ',' FExpr '}'
    {
      $$[0] = $2;  $$[1] = $4;  $$[2] = $6;  $$[3] = 0.0; $$[4] = 1.0;
    }
  | '(' FExpr ',' FExpr ',' FExpr ')'
    {
      $$[0] = $2;  $$[1] = $4;  $$[2] = $6;  $$[3] = 0.0; $$[4] = 1.0;
    }
;

RecursiveListOfListOfDouble :
    ListOfDouble
    {
      $$ = List_Create(2, 1, sizeof(List_T*));
      List_Add($$, &($1));
    }
  | RecursiveListOfListOfDouble ',' ListOfDouble
    {
      List_Add($$, &($3));
    }
;

ListOfDouble :
    FExpr
    {
      $$ = List_Create(2, 1, sizeof(double));
      List_Add($$, &($1));
    }
  | FExpr_Multi
    {
      $$ = $1;
    }
  | '{' '}'
    {
      // creates an empty list
      $$ = List_Create(2, 1, sizeof(double));
    }
  | '{' RecursiveListOfDouble '}'
    {
      $$ = $2;
    }
  | '-' '{' RecursiveListOfDouble '}'
    {
      $$ = $3;
      for(int i = 0; i < List_Nbr($$); i++){
	double *pd = (double*)List_Pointer($$, i);
	(*pd) = - (*pd);
      }
    }
  | FExpr '*' '{' RecursiveListOfDouble '}'
    {
      $$ = $4;
      for(int i = 0; i < List_Nbr($$); i++){
	double *pd = (double*)List_Pointer($$, i);
	(*pd) *= $1;
      }
    }
;

FExpr_Multi :
    '-' FExpr_Multi %prec UNARYPREC
    {
      $$ = $2;
      for(int i = 0; i < List_Nbr($$); i++){
	double *pd = (double*)List_Pointer($$, i);
	(*pd) = - (*pd);
      }
    }
  | FExpr '*' FExpr_Multi
    {
      $$ = $3;
      for(int i = 0; i < List_Nbr($$); i++){
	double *pd = (double*)List_Pointer($$, i);
	(*pd) *= $1;
      }
    }
  | FExpr tDOTS FExpr
    { 
      $$ = List_Create(2, 1, sizeof(double)); 
      for(double d = $1; ($1 < $3) ? (d <= $3) : (d >= $3); ($1 < $3) ? (d += 1.) : (d -= 1.)) 
	List_Add($$, &d);
    }
  | FExpr tDOTS FExpr tDOTS FExpr
    {
      $$ = List_Create(2, 1, sizeof(double)); 
      if(!$5 || ($1 < $3 && $5 < 0) || ($1 > $3 && $5 > 0)){
        yymsg(0, "Wrong increment in '%g:%g:%g'", $1, $3, $5);
	List_Add($$, &($1));
      }
      else
	for(double d = $1; ($5 > 0) ? (d <= $3) : (d >= $3); d += $5)
	  List_Add($$, &d);
   }
  | tPoint '{' FExpr '}'
    {
      // Returns the coordinates of a point and fills a list with it.
      // This allows to ensure e.g. that relative point positions are
      // always conserved
      Vertex *v = FindPoint((int)$3);
      $$ = List_Create(3, 1, sizeof(double));      
      if(!v) {
	yymsg(0, "Unknown point '%d'", (int)$3);
	double d = 0.0;
	List_Add($$, &d);
	List_Add($$, &d);
	List_Add($$, &d);
      }
      else{
	List_Add($$, &v->Pos.X);
	List_Add($$, &v->Pos.Y);
	List_Add($$, &v->Pos.Z);
      }
    }
  | Transform
    {
      $$ = List_Create(List_Nbr($1), 1, sizeof(double));
      for(int i = 0; i < List_Nbr($1); i++){
	Shape *s = (Shape*) List_Pointer($1, i);
	double d = s->Num;
	List_Add($$, &d);
      }
      List_Delete($1);
    }
  | Extrude
    {
      $$ = List_Create(List_Nbr($1), 1, sizeof(double));
      for(int i = 0; i < List_Nbr($1); i++){
	Shape *s = (Shape*) List_Pointer($1, i);
	double d = s->Num;
	List_Add($$, &d);
      }
      List_Delete($1);
    }
  | tSTRING '[' ']'
    {
      $$ = List_Create(2, 1, sizeof(double));
      Symbol TheSymbol, *pSymbol;
      TheSymbol.Name = $1;
      if(!(pSymbol = (Symbol*)Tree_PQuery(Symbol_T, &TheSymbol))) {
	yymsg(0, "Unknown variable '%s'", $1);
	double d = 0.0;
	List_Add($$, &d);
      }
      else{
	for(int i = 0; i < List_Nbr(pSymbol->val); i++)
	  List_Add($$, (double*)List_Pointer_Fast(pSymbol->val, i));
      }
      Free($1);
    }
  | tSTRING '[' '{' RecursiveListOfDouble '}' ']'
    {
      $$ = List_Create(2, 1, sizeof(double));
      Symbol TheSymbol, *pSymbol;
      TheSymbol.Name = $1;
      if(!(pSymbol = (Symbol*)Tree_PQuery(Symbol_T, &TheSymbol))) {
	yymsg(0, "Unknown variable '%s'", $1);
	double d = 0.0;
	List_Add($$, &d);
      }
      else{
	for(int i = 0; i < List_Nbr($4); i++){
	  int j = (int)(*(double*)List_Pointer_Fast($4, i));
	  double *pd;
	  if((pd = (double*)List_Pointer_Test(pSymbol->val, j)))
	    List_Add($$, pd);
	  else
	    yymsg(0, "Uninitialized variable '%s[%d]'", $1, j);	  
	}
      }
      Free($1);
      List_Delete($4);
    }
;

RecursiveListOfDouble :
    FExpr
    {
      $$ = List_Create(2, 1, sizeof(double));
      List_Add($$, &($1));
    }
  | FExpr_Multi
    {
      $$ = $1;
    }
  | RecursiveListOfDouble ',' FExpr
    {
      List_Add($$, &($3));
    }
  | RecursiveListOfDouble ',' FExpr_Multi
    {
      for(int i = 0; i < List_Nbr($3); i++){
	double d;
	List_Read($3, i, &d);
	List_Add($$, &d);
      }
      List_Delete($3);
    }
;

ColorExpr :
    '{' FExpr ',' FExpr ',' FExpr ',' FExpr '}'
    {
      $$ = CTX.PACK_COLOR((int)$2, (int)$4, (int)$6, (int)$8);
    }
  | '{' FExpr ',' FExpr ',' FExpr '}'
    {
      $$ = CTX.PACK_COLOR((int)$2, (int)$4, (int)$6, 255);
    }
/* shift/reduce conflict
  | '{' tSTRING ',' FExpr '}'
    {
      int flag;
      $$ = Get_ColorForString(ColorString, (int)$4, $2, &flag);
      if(flag) yymsg(0, "Unknown color '%s'", $2);
    }
*/
  | tSTRING
    {
      int flag;
      $$ = Get_ColorForString(ColorString, -1, $1, &flag);
      if(flag) yymsg(0, "Unknown color '%s'", $1);
      Free($1);
    }
  | tSTRING '.' tColor '.' tSTRING 
    {
      unsigned int val = 0;
      ColorOption(GMSH_GET, $1, 0, $5, val);
      $$ = val;
      Free($1); Free($5);
    }
;

ListOfColor :
    '{' RecursiveListOfColor '}'
    {
      $$ = $2;
    }
  | tSTRING '[' FExpr ']' '.' tColorTable
    {
      $$ = List_Create(256, 10, sizeof(unsigned int));
      GmshColorTable *ct = Get_ColorTable((int)$3);
      if(!ct)
	yymsg(0, "View[%d] does not exist", (int)$3);
      else{
	for(int i = 0; i < ct->size; i++) 
	  List_Add($$, &ct->table[i]);
      }
      Free($1);
    }
;

RecursiveListOfColor :
    ColorExpr
    {
      $$ = List_Create(256, 10, sizeof(unsigned int));
      List_Add($$, &($1));
    }
  | RecursiveListOfColor ',' ColorExpr
    {
      List_Add($$, &($3));
    }
;

StringExprVar :
    StringExpr
    {
      $$ = $1;
    }
  | tSTRING
    {
      Msg::Warning("Named string expressions not implemented yet");
    }
;

StringExpr :
    tBIGSTR
    {
      $$ = $1;
    }
  | tToday
    {
      $$ = (char *)Malloc(32*sizeof(char));
      time_t now;
      time(&now);
      strcpy($$, ctime(&now));
      $$[strlen($$) - 1] = '\0';
    }
  | tStrCat '(' StringExprVar ',' StringExprVar ')'
    {
      $$ = (char *)Malloc((strlen($3)+strlen($5)+1)*sizeof(char));
      strcpy($$, $3);
      strcat($$, $5);
      Free($3);
      Free($5);
    }
  | tStrPrefix '(' StringExprVar ')'
    {
      $$ = (char *)Malloc((strlen($3)+1)*sizeof(char));
      int i;
      for(i = strlen($3) - 1; i >= 0; i--){
	if($3[i] == '.'){
	  strncpy($$, $3, i);
	  $$[i]='\0';
	  break;
	}
      }
      if(i <= 0) strcpy($$, $3);
      Free($3);
    }
  | tStrRelative '(' StringExprVar ')'
    {
      $$ = (char *)Malloc((strlen($3)+1)*sizeof(char));
      int i;
      for(i = strlen($3) - 1; i >= 0; i--){
	if($3[i] == '/' || $3[i] == '\\')
	  break;
      }
      if(i <= 0)
	strcpy($$, $3);
      else
	strcpy($$, &$3[i+1]);
      Free($3);
    }
  | tSprintf '(' StringExprVar ')'
    {
      $$ = $3;
    }
  | tSprintf '(' StringExprVar ',' RecursiveListOfDouble ')'
    {
      char tmpstring[1024];
      int i = PrintListOfDouble($3, $5, tmpstring);
      if(i < 0){
	yymsg(0, "Too few arguments in Sprintf");
	$$ = $3;
      }
      else if(i > 0){
	yymsg(0, "%d extra argument%s in Sprintf", i, (i > 1) ? "s" : "");
	$$ = $3;
      }
      else{
	$$ = (char*)Malloc((strlen(tmpstring)+1)*sizeof(char));
	strcpy($$, tmpstring);
	Free($3);
      }
      List_Delete($5);
    }
  | tSprintf '(' tSTRING '.' tSTRING ')'
    { 
      const char *val = "";
      StringOption(GMSH_GET, $3, 0, $5, val);
      $$ = (char*)Malloc((strlen(val) + 1) * sizeof(char));
      strcpy($$, val);
      Free($3); Free($5);
    }
  | tSprintf '(' tSTRING '[' FExpr ']' '.' tSTRING ')'
    { 
      const char *val = "";
      StringOption(GMSH_GET, $3, (int)$5, $8, val);
      $$ = (char*)Malloc((strlen(val) + 1) * sizeof(char));
      strcpy($$, val);
      Free($3); Free($8);
    }
;

%%

void DeleteSymbol(void *a, void *b)
{
  Symbol *s = (Symbol*)a;
  Free(s->Name);
  List_Delete(s->val);
}

int CompareSymbols (const void *a, const void *b)
{
  return(strcmp(((Symbol*)a)->Name, ((Symbol*)b)->Name));
}

void InitSymbols()
{
  if(Symbol_T){
    Tree_Action(Symbol_T, DeleteSymbol);
    Tree_Delete(Symbol_T);
  }
  Symbol_T = Tree_Create(sizeof(Symbol), CompareSymbols);
}

int PrintListOfDouble(char *format, List_T *list, char *buffer)
{
  int j, k;
  char tmp1[256], tmp2[256];

  j = 0;
  buffer[j] = '\0';

  while(j < (int)strlen(format) && format[j] != '%') j++;
  strncpy(buffer, format, j); 
  buffer[j]='\0'; 
  for(int i = 0; i < List_Nbr(list); i++){
    k = j;
    j++;
    if(j < (int)strlen(format)){
      if(format[j] == '%'){
	strcat(buffer, "%");
	j++;
      }
      while(j < (int)strlen(format) && format[j] != '%') j++;
      if(k != j){
	strncpy(tmp1, &(format[k]), j-k);
	tmp1[j-k] = '\0';
	sprintf(tmp2, tmp1, *(double*)List_Pointer(list, i)); 
	strcat(buffer, tmp2);
      }
    }
    else
      return List_Nbr(list)-i;
  }
  if(j != (int)strlen(format))
    return -1;
  return 0;
}

void FixRelativePath(const char *in, char *out)
{
  if(in[0] == '/' || in[0] == '\\' || (strlen(in)>2 && in[1] == ':')){
    // do nothing: 'in' is an absolute path
    strcpy(out, in);
  }
  else{
    // append 'in' to the path of the parent file
    strcpy(out, gmsh_yyname);
    int i = strlen(out) - 1 ;
    while(i >= 0 && gmsh_yyname[i] != '/' && gmsh_yyname[i] != '\\') i-- ;
    out[i+1] = '\0';
    strcat(out, in);
  }
}

void yyerror(char *s)
{
  Msg::Error("'%s', line %d : %s (%s)", gmsh_yyname, gmsh_yylineno - 1, s, gmsh_yytext);
  gmsh_yyerrorstate++;
}

void yymsg(int level, const char *fmt, ...)
{
  va_list args;
  char tmp[1024];

  va_start(args, fmt);
  vsprintf(tmp, fmt, args);
  va_end(args);

  if(level == 0){
    Msg::Error("'%s', line %d : %s", gmsh_yyname, gmsh_yylineno - 1, tmp);
    gmsh_yyerrorstate++;
  }
  else
    Msg::Warning("'%s', line %d : %s", gmsh_yyname, gmsh_yylineno - 1, tmp);
}
