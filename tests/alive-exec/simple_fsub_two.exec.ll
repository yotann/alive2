define float @foo(float %0) #0 {
  %2 = fsub float %0, 2.0
  ret float %2
}

; CHECK: ConcreteVal( poison=0, 32b, 1F)
; CHECK: ConcreteVal( poison=0, 32b, 1F)