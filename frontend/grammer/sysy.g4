grammar sysy;

// FIXME: adapt grammer from c1 to sysy

compilationUnit: (decl | funcdef)+;

decl: (constdecl | vardecl) SemiColon;

constdecl: Const (Int | Float) constdef (Comma constdef)*;

constdef: Identifier Assign exp
        | Identifier LeftBracket exp RightBracket Assign LeftBrace exp (Comma exp)* RightBrace;

vardecl: (Int | Float) vardef (Comma vardef)*;

vardef: Identifier (Assign exp)?
      | Identifier LeftBracket exp RightBracket (Assign LeftBrace exp (Comma exp)* RightBrace)?;

funcdef: Void Identifier LeftParen RightParen block;

block: LeftBrace (decl | stmt)* RightBrace;

stmt: lval Assign exp SemiColon
    | Identifier LeftParen RightParen SemiColon
    | block
    | If LeftParen cond RightParen stmt (Else stmt)?
    | While LeftParen cond RightParen stmt
    | SemiColon;

lval: Identifier
    | Identifier LeftBracket exp RightBracket;

cond: exp (Equal | NonEqual | Less | Greater | LessEqual | GreaterEqual) exp;

exp:
    (Plus | Minus) exp
    | exp (Multiply | Divide | Modulo) exp
    | exp (Plus | Minus) exp
    | LeftParen exp RightParen
    | number
    | lval
;

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
Const: 'const';
Equal: '==';
NonEqual: '!=';
Less: '<';
Greater: '>';
LessEqual: '<=';
GreaterEqual: '>=';
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
