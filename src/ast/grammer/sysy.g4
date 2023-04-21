grammar sysy;

compUnit: (decl | funcdef)+;

decl: (constdecl | vardecl) SemiColon;

constdecl: Const (Int | Float) constdef (Comma constdef)*;

constdef: Identifier (LeftBracket constexp RightBracket)* Assign constInit;

constInit: LeftBrace constInit (Comma constInit)* RightBrace
         | constexp;

vardecl: (Int | Float) vardef (Comma vardef)*;

vardef: Identifier (LeftBracket constexp RightBracket)* (Assign varInit)?;

varInit: LeftBrace varInit (Comma varInit)* RightBrace
       | exp;

funcdef: (Void | Int | Float) Identifier LeftParen (funcparam (Comma funcparam)*)? RightParen block;

funcparam: (Int | Float) Identifier (LeftBracket RightBracket (LeftBracket constexp RightBracket)*)?;

block: LeftBrace (decl | stmt)* RightBrace;

// semicolon is not a stmt
stmt: lval Assign exp SemiColon
    | exp? SemiColon
    | block
    | If LeftParen cond RightParen stmt (Else stmt)?
    | While LeftParen cond RightParen stmt
    | Break SemiColon
    | Continue SemiColon
    | Return exp* SemiColon;

lval: Identifier (LeftBracket exp RightBracket)*;

// not cond is not allowed
cond: cond (Less | Greater | LessEqual | GreaterEqual) cond 
    | cond (Equal | NonEqual) cond 
    | cond And cond 
    | cond Or cond 
    | exp;

// not is only allowed in cond
exp: (Plus | Minus | Not) exp
   | exp (Multiply | Divide | Modulo) exp
   | exp (Plus | Minus) exp
   | LeftParen exp RightParen
   | number
   | lval
   | Identifier LeftParen (exp (Comma exp)*)? RightParen;

// identifiers in a constexp should all be const
constexp: exp;

number: FloatConst
      | IntConst;

Comma: ',';
SemiColon: ';';
Assign: '=';
LeftBracket: '[';
RightBracket: ']';
LeftBrace: '{';
RightBrace: '}';
LeftParen: '(';
RightParen: ')';
If: 'if';
Else: 'else';
While: 'while';
Break: 'break';
Continue: 'continue';
Return: 'return';
Const: 'const';
Equal: '==';
NonEqual: '!=';
Less: '<';
Greater: '>';
LessEqual: '<=';
GreaterEqual: '>=';
Not: '!';
And: '&&';
Or: '||';
Plus: '+';
Minus: '-';
Multiply: '*';
Divide: '/';
Modulo: '%';

Int: 'int';
Float: 'float';
Void: 'void';

Identifier: [_a-zA-Z] [_0-9a-zA-Z]*;

IntConst: '0' [0-7]*
        | [1-9] [0-9]*
        | '0' [xX] [0-9a-fA-F]+;

FloatConst: ([0-9]* '.' [0-9]+ | [0-9]+ '.') ([eE] [-+]? [0-9]+)?
          | [0-9]+ [eE] [-+]? [0-9]+;

LineComment: ('//' | '/\\' ('\r'? '\n') '/') ~[\r\n\\]* ('\\' ('\r'? '\n')? ~[\r\n\\]*)* '\r'? '\n' -> skip;
BlockComment: '/*' .*? '*/' -> skip;

WhiteSpace: [ \t\r\n]+ -> skip;
