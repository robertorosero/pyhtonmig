;
; Copyright (c) 2008-2010 Stefan Krah. All Rights Reserved.
; Licensed to PSF under a Contributor Agreement.
;


PUBLIC	_mpd_div_words
_TEXT	SEGMENT
q$ = 8
r$ = 16
hi$ = 24
lo$ = 32
d$ = 40
_mpd_div_words PROC
	mov	r10, rdx
	mov	rdx, r8
	mov	rax, r9
	div	QWORD PTR d$[rsp]
	mov	QWORD PTR [r10], rdx
	mov	QWORD PTR [rcx], rax
	ret	0
_mpd_div_words ENDP
_TEXT	ENDS
END


