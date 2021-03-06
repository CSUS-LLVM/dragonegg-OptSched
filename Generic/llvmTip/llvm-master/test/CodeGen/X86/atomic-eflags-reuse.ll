; NOTE: Assertions have been autogenerated by utils/update_llc_test_checks.py
; RUN: llc < %s -mtriple=x86_64-- | FileCheck %s

define i32 @test_add_1_cmov_slt(i64* %p, i32 %a0, i32 %a1) #0 {
; CHECK-LABEL: test_add_1_cmov_slt:
; CHECK:       # BB#0: # %entry
; CHECK-NEXT:    lock incq (%rdi)
; CHECK-NEXT:    cmovgl %edx, %esi
; CHECK-NEXT:    movl %esi, %eax
; CHECK-NEXT:    retq
entry:
  %tmp0 = atomicrmw add i64* %p, i64 1 seq_cst
  %tmp1 = icmp slt i64 %tmp0, 0
  %tmp2 = select i1 %tmp1, i32 %a0, i32 %a1
  ret i32 %tmp2
}

define i32 @test_add_1_cmov_sge(i64* %p, i32 %a0, i32 %a1) #0 {
; CHECK-LABEL: test_add_1_cmov_sge:
; CHECK:       # BB#0: # %entry
; CHECK-NEXT:    lock incq (%rdi)
; CHECK-NEXT:    cmovlel %edx, %esi
; CHECK-NEXT:    movl %esi, %eax
; CHECK-NEXT:    retq
entry:
  %tmp0 = atomicrmw add i64* %p, i64 1 seq_cst
  %tmp1 = icmp sge i64 %tmp0, 0
  %tmp2 = select i1 %tmp1, i32 %a0, i32 %a1
  ret i32 %tmp2
}

define i32 @test_sub_1_cmov_sle(i64* %p, i32 %a0, i32 %a1) #0 {
; CHECK-LABEL: test_sub_1_cmov_sle:
; CHECK:       # BB#0: # %entry
; CHECK-NEXT:    lock decq (%rdi)
; CHECK-NEXT:    cmovgel %edx, %esi
; CHECK-NEXT:    movl %esi, %eax
; CHECK-NEXT:    retq
entry:
  %tmp0 = atomicrmw sub i64* %p, i64 1 seq_cst
  %tmp1 = icmp sle i64 %tmp0, 0
  %tmp2 = select i1 %tmp1, i32 %a0, i32 %a1
  ret i32 %tmp2
}

define i32 @test_sub_1_cmov_sgt(i64* %p, i32 %a0, i32 %a1) #0 {
; CHECK-LABEL: test_sub_1_cmov_sgt:
; CHECK:       # BB#0: # %entry
; CHECK-NEXT:    lock decq (%rdi)
; CHECK-NEXT:    cmovll %edx, %esi
; CHECK-NEXT:    movl %esi, %eax
; CHECK-NEXT:    retq
entry:
  %tmp0 = atomicrmw sub i64* %p, i64 1 seq_cst
  %tmp1 = icmp sgt i64 %tmp0, 0
  %tmp2 = select i1 %tmp1, i32 %a0, i32 %a1
  ret i32 %tmp2
}

; FIXME: (setcc slt x, 0) gets combined into shr early.
define i8 @test_add_1_setcc_slt(i64* %p) #0 {
; CHECK-LABEL: test_add_1_setcc_slt:
; CHECK:       # BB#0: # %entry
; CHECK-NEXT:    movl $1, %eax
; CHECK-NEXT:    lock xaddq %rax, (%rdi)
; CHECK-NEXT:    shrq $63, %rax
; CHECK-NEXT:    # kill: %AL<def> %AL<kill> %RAX<kill>
; CHECK-NEXT:    retq
entry:
  %tmp0 = atomicrmw add i64* %p, i64 1 seq_cst
  %tmp1 = icmp slt i64 %tmp0, 0
  %tmp2 = zext i1 %tmp1 to i8
  ret i8 %tmp2
}

define i8 @test_sub_1_setcc_sgt(i64* %p) #0 {
; CHECK-LABEL: test_sub_1_setcc_sgt:
; CHECK:       # BB#0: # %entry
; CHECK-NEXT:    lock decq (%rdi)
; CHECK-NEXT:    setge %al
; CHECK-NEXT:    retq
entry:
  %tmp0 = atomicrmw sub i64* %p, i64 1 seq_cst
  %tmp1 = icmp sgt i64 %tmp0, 0
  %tmp2 = zext i1 %tmp1 to i8
  ret i8 %tmp2
}

define i32 @test_add_1_brcond_sge(i64* %p, i32 %a0, i32 %a1) #0 {
; CHECK-LABEL: test_add_1_brcond_sge:
; CHECK:       # BB#0: # %entry
; CHECK-NEXT:    lock incq (%rdi)
; CHECK-NEXT:    jle .LBB6_2
; CHECK-NEXT:  # BB#1: # %t
; CHECK-NEXT:    movl %esi, %eax
; CHECK-NEXT:    retq
; CHECK-NEXT:  .LBB6_2: # %f
; CHECK-NEXT:    movl %edx, %eax
; CHECK-NEXT:    retq
entry:
  %tmp0 = atomicrmw add i64* %p, i64 1 seq_cst
  %tmp1 = icmp sge i64 %tmp0, 0
  br i1 %tmp1, label %t, label %f
t:
  ret i32 %a0
f:
  ret i32 %a1
}

; Also make sure we don't muck with condition codes that we should ignore.
; No need to test unsigned comparisons, as they should all be simplified.

define i32 @test_add_1_cmov_sle(i64* %p, i32 %a0, i32 %a1) #0 {
; CHECK-LABEL: test_add_1_cmov_sle:
; CHECK:       # BB#0: # %entry
; CHECK-NEXT:    movl $1, %eax
; CHECK-NEXT:    lock xaddq %rax, (%rdi)
; CHECK-NEXT:    testq %rax, %rax
; CHECK-NEXT:    cmovgl %edx, %esi
; CHECK-NEXT:    movl %esi, %eax
; CHECK-NEXT:    retq
entry:
  %tmp0 = atomicrmw add i64* %p, i64 1 seq_cst
  %tmp1 = icmp sle i64 %tmp0, 0
  %tmp2 = select i1 %tmp1, i32 %a0, i32 %a1
  ret i32 %tmp2
}

define i32 @test_add_1_cmov_sgt(i64* %p, i32 %a0, i32 %a1) #0 {
; CHECK-LABEL: test_add_1_cmov_sgt:
; CHECK:       # BB#0: # %entry
; CHECK-NEXT:    movl $1, %eax
; CHECK-NEXT:    lock xaddq %rax, (%rdi)
; CHECK-NEXT:    testq %rax, %rax
; CHECK-NEXT:    cmovlel %edx, %esi
; CHECK-NEXT:    movl %esi, %eax
; CHECK-NEXT:    retq
entry:
  %tmp0 = atomicrmw add i64* %p, i64 1 seq_cst
  %tmp1 = icmp sgt i64 %tmp0, 0
  %tmp2 = select i1 %tmp1, i32 %a0, i32 %a1
  ret i32 %tmp2
}

; Test a result being used by more than just the comparison.

define i8 @test_add_1_setcc_sgt_reuse(i64* %p, i64* %p2) #0 {
; CHECK-LABEL: test_add_1_setcc_sgt_reuse:
; CHECK:       # BB#0: # %entry
; CHECK-NEXT:    movl $1, %ecx
; CHECK-NEXT:    lock xaddq %rcx, (%rdi)
; CHECK-NEXT:    testq %rcx, %rcx
; CHECK-NEXT:    setg %al
; CHECK-NEXT:    movq %rcx, (%rsi)
; CHECK-NEXT:    retq
entry:
  %tmp0 = atomicrmw add i64* %p, i64 1 seq_cst
  %tmp1 = icmp sgt i64 %tmp0, 0
  %tmp2 = zext i1 %tmp1 to i8
  store i64 %tmp0, i64* %p2
  ret i8 %tmp2
}

define i8 @test_sub_2_setcc_sgt(i64* %p) #0 {
; CHECK-LABEL: test_sub_2_setcc_sgt:
; CHECK:       # BB#0: # %entry
; CHECK-NEXT:    movq $-2, %rax
; CHECK-NEXT:    lock xaddq %rax, (%rdi)
; CHECK-NEXT:    testq %rax, %rax
; CHECK-NEXT:    setg %al
; CHECK-NEXT:    retq
entry:
  %tmp0 = atomicrmw sub i64* %p, i64 2 seq_cst
  %tmp1 = icmp sgt i64 %tmp0, 0
  %tmp2 = zext i1 %tmp1 to i8
  ret i8 %tmp2
}

attributes #0 = { nounwind }
