define i32 @src(i1 %c, i32* %p) {
ENTRY:
  br i1 %c, label %A, label %EXIT
A:
  %v1 = load i32, i32* %p, align 4
  br label %EXIT
EXIT:
  %val = phi i32 [%v1, %A], [0, %ENTRY]
  call void @f(i32* dereferenceable(4) %p) ; does not guarantee p is 4-bytes aligned
  ret i32 %val
}

define i32 @tgt(i1 %c, i32* %p) {
  %v1 = load i32, i32* %p, align 4
  %val = select i1 %c, i32 %v1, i32 0
  call void @f(i32* dereferenceable(4) %p)
  ret i32 %val
}

declare void @f(i32* %ptr)

; ERROR: Source is more defined than target
; XFAIL: call with escaped locals
; SKIP-IDENTITY
