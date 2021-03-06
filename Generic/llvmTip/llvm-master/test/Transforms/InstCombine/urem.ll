; NOTE: Assertions have been autogenerated by utils/update_test_checks.py
; RUN: opt < %s -instcombine -S | FileCheck %s

define i64 @rem_unsigned(i64 %x1, i64 %y2) {
; CHECK-LABEL: @rem_unsigned(
; CHECK-NEXT:    [[R:%.*]] = urem i64 %x1, %y2
; CHECK-NEXT:    ret i64 [[R]]
;
  %r = udiv i64 %x1, %y2
  %r7 = mul i64 %r, %y2
  %r8 = sub i64 %x1, %r7
  ret i64 %r8
}

; PR28672 - https://llvm.org/bugs/show_bug.cgi?id=28672

define i8 @big_divisor(i8 %x) {
; CHECK-LABEL: @big_divisor(
; CHECK-NEXT:    [[TMP1:%.*]] = icmp ult i8 %x, -127
; CHECK-NEXT:    [[TMP2:%.*]] = add i8 %x, 127
; CHECK-NEXT:    [[REM:%.*]] = select i1 [[TMP1]], i8 %x, i8 [[TMP2]]
; CHECK-NEXT:    ret i8 [[REM]]
;
  %rem = urem i8 %x, 129
  ret i8 %rem
}

define i5 @biggest_divisor(i5 %x) {
; CHECK-LABEL: @biggest_divisor(
; CHECK-NEXT:    [[NOT_:%.*]] = icmp eq i5 %x, -1
; CHECK-NEXT:    [[TMP1:%.*]] = zext i1 [[NOT_]] to i5
; CHECK-NEXT:    [[REM:%.*]] = add i5 [[TMP1]], %x
; CHECK-NEXT:    ret i5 [[REM]]
;
  %rem = urem i5 %x, -1
  ret i5 %rem
}

; TODO: Should vector subtract of constant be canonicalized to add?
define <2 x i4> @big_divisor_vec(<2 x i4> %x) {
; CHECK-LABEL: @big_divisor_vec(
; CHECK-NEXT:    [[TMP1:%.*]] = icmp ult <2 x i4> %x, <i4 -3, i4 -3>
; CHECK-NEXT:    [[TMP2:%.*]] = sub <2 x i4> %x, <i4 -3, i4 -3>
; CHECK-NEXT:    [[REM:%.*]] = select <2 x i1> [[TMP1]], <2 x i4> %x, <2 x i4> [[TMP2]]
; CHECK-NEXT:    ret <2 x i4> [[REM]]
;
  %rem = urem <2 x i4> %x, <i4 13, i4 13>
  ret <2 x i4> %rem
}

