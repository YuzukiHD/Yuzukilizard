# long __ashldi3 (long A, int B)

    .align  2
	.global __ashldi3
	.type   __ashldi3, %function
__ashldi3:
	beqz	a2, .L2
	li	a5,32
	sub	a5,a5,a2
	bgtz	a5,.L3
	addi	a1,a2,-32
	li	a4,0
	sll	a1,a0,a1
	mv	a0,a4
.L2:
	ret
.L3:
	sll	a4,a0,a2
	sll	a1,a1,a2
	srl	a0,a0,a5
	or	a1,a1,a0
	mv	a0,a4
	ret
	.size   __ashldi3, . - __ashldi3
