
declare i8   @llvm.ctlz.i8  (i8, i1)
declare i16  @llvm.ctlz.i16 (i16, i1)
declare i32  @llvm.ctlz.i32 (i32, i1)

define i8 @foo(i8 %0) {
  %2 = call i8 @llvm.ctlz.i8(i8 %0, i1 0)
  ret i8 %2
}

; CHECK: ConcreteVal( poison=0, 8b, 6u 6s)
; CHECK: ConcreteVal( poison=0, 8b, 6u 6s)

define i16 @bar(i16 %0) {
  %2 = call i16 @llvm.ctlz.i16(i16 %0, i1 0)
  ret i16 %2
}

; CHECK: ConcreteVal( poison=0, 16b, 14u 14s)
; CHECK: ConcreteVal( poison=0, 16b, 14u 14s)

define i32 @baz(i32 %0) {
  %2 = call i32 @llvm.ctlz.i32(i32 %0, i1 0)
  ret i32 %2
}

; CHECK: ConcreteVal( poison=0, 32b, 30u 30s)
; CHECK: ConcreteVal( poison=0, 32b, 30u 30s)

