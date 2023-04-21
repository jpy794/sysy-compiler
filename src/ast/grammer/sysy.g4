grammar sysy;

compUnit: (vardecl | funcdef)+;

vardecl: Const? (Int | Float) vardef (Comma vardef)* SemiColon;

vardef: Identifier (LeftBracket exp RightBracket)* (Assign varInit)?;

varInit: LeftBrace varInit (Comma varInit)* RightBrace
       | exp;

funcdef: (Void | Int | Float) Identifier LeftParen (funcparam (Comma funcparam)*)? RightParen block;

funcparam: (Int | Float) Identifier (LeftBracket RightBracket (LeftBracket exp RightBracket)*)?;

block: LeftBrace (stmt)* RightBrace;

expStmt: exp? SemiColon;

// semicolon is not a stmt
stmt: lval Assign exp SemiColon
    | expStmt
    | block
    | If LeftParen exp stmt (Else stmt)?
    | While LeftParen exp RightParen stmt
    | Break SemiColon
    | Continue SemiColon
    | Return exp* SemiColon
    | vardecl;

lval: Identifier (LeftBracket exp RightBracket)*;

funcCall: Identifier LeftParen (exp (Comma exp)*)? RightParen;

parenExp: LeftParen exp RightParen;

// not is only allowed in cond
exp: (Plus | Minus | Not) exp
   | exp (Multiply | Divide | Modulo) exp
   | exp (Plus | Minus) exp
   | exp (Less | Greater | LessEqual | GreaterEqual) exp
   | exp (Equal | NonEqual) exp
   | exp And exp
   | exp Or exp
   | parenExp
   | number
   | lval
   | funcCall;

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
