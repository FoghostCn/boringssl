.text
BORINGSSL_bcm_text_start:
	.file	"foo.c"
	.abiversion 2
	.section	".toc","aw"
# WAS .section	".text"
.text
	.section	".toc","aw"
.LC0:

	.quad	stderr
.LC3:

	.quad	kExportedString
.LC6:

	.quad	exported_function
# WAS .section	".text"
.text
	.align 2
	.p2align 4,,15
	.globl exported_function
	.type	exported_function, @function
.Lexported_function_local_target:
exported_function:
0:
999:
	addis 2, 12, .LBORINGSSL_external_toc-999b@ha
	addi 2, 2, .LBORINGSSL_external_toc-999b@l
	ld 12, 0(2)
	add 2, 2, 12
# WAS addi 2,2,.TOC.-0b@l
	.localentry	exported_function,.-exported_function
.Lexported_function_local_entry:
	mflr 0
	std 19,-104(1)
	std 20,-96(1)
	std 21,-88(1)
	std 22,-80(1)
# WAS addis 21,2,.LC1@toc@ha
	addi 1, 1, -288
	std 3, -8(1)
	mflr 3
	std 3, -16(1)
	std 2, -24(1)
	bl .Lbcm_loadtoc__dot_LC1_at_toc_at_ha
	ld 2, -24(1)
	add	21, 2, 3
	ld 3, -16(1)
	mtlr 3
	ld 3, -8(1)
	addi 1, 1, 288
# WAS addis 22,2,.LC2@toc@ha
	addi 1, 1, -288
	std 3, -8(1)
	mflr 3
	std 3, -16(1)
	std 2, -24(1)
	bl .Lbcm_loadtoc__dot_LC2_at_toc_at_ha
	ld 2, -24(1)
	add	22, 2, 3
	ld 3, -16(1)
	mtlr 3
	ld 3, -8(1)
	addi 1, 1, 288
	std 23,-72(1)
	std 24,-64(1)
# WAS addis 23,2,.LC4@toc@ha
	addi 1, 1, -288
	std 3, -8(1)
	mflr 3
	std 3, -16(1)
	std 2, -24(1)
	bl .Lbcm_loadtoc__dot_LC4_at_toc_at_ha
	ld 2, -24(1)
	add	23, 2, 3
	ld 3, -16(1)
	mtlr 3
	ld 3, -8(1)
	addi 1, 1, 288
# WAS addis 24,2,function@toc@ha
	addi 1, 1, -288
	std 3, -8(1)
	mflr 3
	std 3, -16(1)
	std 2, -24(1)
	bl .Lbcm_loadtoc__dot_Lfunction_local_target_at_toc_at_ha
	ld 2, -24(1)
	add	24, 2, 3
	ld 3, -16(1)
	mtlr 3
	ld 3, -8(1)
	addi 1, 1, 288
	std 25,-56(1)
	std 26,-48(1)
# WAS addis 25,2,.LC5@toc@ha
	addi 1, 1, -288
	std 3, -8(1)
	mflr 3
	std 3, -16(1)
	std 2, -24(1)
	bl .Lbcm_loadtoc__dot_LC5_at_toc_at_ha
	ld 2, -24(1)
	add	25, 2, 3
	ld 3, -16(1)
	mtlr 3
	ld 3, -8(1)
	addi 1, 1, 288
# WAS addis 26,2,.LC7@toc@ha
	addi 1, 1, -288
	std 3, -8(1)
	mflr 3
	std 3, -16(1)
	std 2, -24(1)
	bl .Lbcm_loadtoc__dot_LC7_at_toc_at_ha
	ld 2, -24(1)
	add	26, 2, 3
	ld 3, -16(1)
	mtlr 3
	ld 3, -8(1)
	addi 1, 1, 288
	std 27,-40(1)
	std 28,-32(1)
# WAS addis 28,2,.LC8@toc@ha
	addi 1, 1, -288
	std 3, -8(1)
	mflr 3
	std 3, -16(1)
	std 2, -24(1)
	bl .Lbcm_loadtoc__dot_LC8_at_toc_at_ha
	ld 2, -24(1)
	add	28, 2, 3
	ld 3, -16(1)
	mtlr 3
	ld 3, -8(1)
	addi 1, 1, 288
# WAS addi 21,21,.LC1@toc@l
	addi 1, 1, -288
	std 3, -8(1)
	mflr 3
	std 3, -16(1)
	std 2, -24(1)
	bl .Lbcm_loadtoc__dot_LC1_at_toc_at_l
	ld 2, -24(1)
	add	21, 21, 3
	ld 3, -16(1)
	mtlr 3
	ld 3, -8(1)
	addi 1, 1, 288
	std 29,-24(1)
	std 30,-16(1)
# WAS addis 29,2,.LANCHOR0@toc@ha
	addi 1, 1, -288
	std 3, -8(1)
	mflr 3
	std 3, -16(1)
	std 2, -24(1)
	bl .Lbcm_loadtoc__dot_LANCHOR0_at_toc_at_ha
	ld 2, -24(1)
	add	29, 2, 3
	ld 3, -16(1)
	mtlr 3
	ld 3, -8(1)
	addi 1, 1, 288
# WAS addi 22,22,.LC2@toc@l
	addi 1, 1, -288
	std 3, -8(1)
	mflr 3
	std 3, -16(1)
	std 2, -24(1)
	bl .Lbcm_loadtoc__dot_LC2_at_toc_at_l
	ld 2, -24(1)
	add	22, 22, 3
	ld 3, -16(1)
	mtlr 3
	ld 3, -8(1)
	addi 1, 1, 288
	std 31,-8(1)
	std 0,16(1)
# WAS addi 29,29,.LANCHOR0@toc@l
	addi 1, 1, -288
	std 3, -8(1)
	mflr 3
	std 3, -16(1)
	std 2, -24(1)
	bl .Lbcm_loadtoc__dot_LANCHOR0_at_toc_at_l
	ld 2, -24(1)
	add	29, 29, 3
	ld 3, -16(1)
	mtlr 3
	ld 3, -8(1)
	addi 1, 1, 288
# WAS addi 23,23,.LC4@toc@l
	addi 1, 1, -288
	std 3, -8(1)
	mflr 3
	std 3, -16(1)
	std 2, -24(1)
	bl .Lbcm_loadtoc__dot_LC4_at_toc_at_l
	ld 2, -24(1)
	add	23, 23, 3
	ld 3, -16(1)
	mtlr 3
	ld 3, -8(1)
	addi 1, 1, 288
	stdu 1,-208(1)
# WAS addis 31,2,.LC0@toc@ha		# gpr load fusion, type long
	addi 1, 1, -288
	std 3, -8(1)
	mflr 3
	std 3, -16(1)
	std 2, -24(1)
	bl .Lbcm_loadtoc__dot_LC0_at_toc_at_ha
	ld 2, -24(1)
	add	31, 2, 3
	ld 3, -16(1)
	mtlr 3
	ld 3, -8(1)
	addi 1, 1, 288
# WAS ld 31,.LC0@toc@l(31)
	addi 1, 1, -288
	std 3, -8(1)
	mflr 3
	std 3, -16(1)
	std 2, -24(1)
	bl .Lbcm_loadtoc__dot_LC0_at_toc_at_l
	ld 2, -24(1)
	add 3, 3, 31
	ld 31, 0(3)
	ld 3, -16(1)
	mtlr 3
	ld 3, -8(1)
	addi 1, 1, 288
# WAS addis 19,2,.LC3@toc@ha		# gpr load fusion, type long
	addi 1, 1, -288
	std 3, -8(1)
	mflr 3
	std 3, -16(1)
	std 2, -24(1)
	bl .Lbcm_loadtoc__dot_LC3_at_toc_at_ha
	ld 2, -24(1)
	add	19, 2, 3
	ld 3, -16(1)
	mtlr 3
	ld 3, -8(1)
	addi 1, 1, 288
# WAS ld 19,.LC3@toc@l(19)
	addi 1, 1, -288
	std 3, -8(1)
	mflr 3
	std 3, -16(1)
	std 2, -24(1)
	bl .Lbcm_loadtoc__dot_LC3_at_toc_at_l
	ld 2, -24(1)
	add 3, 3, 19
	ld 19, 0(3)
	ld 3, -16(1)
	mtlr 3
	ld 3, -8(1)
	addi 1, 1, 288
	addis 30,29,0x5
# WAS addi 24,24,function@toc@l
	addi 1, 1, -288
	std 3, -8(1)
	mflr 3
	std 3, -16(1)
	std 2, -24(1)
	bl .Lbcm_loadtoc__dot_Lfunction_local_target_at_toc_at_l
	ld 2, -24(1)
	add	24, 24, 3
	ld 3, -16(1)
	mtlr 3
	ld 3, -8(1)
	addi 1, 1, 288
# WAS addis 20,2,.LC6@toc@ha		# gpr load fusion, type long
	addi 1, 1, -288
	std 3, -8(1)
	mflr 3
	std 3, -16(1)
	std 2, -24(1)
	bl .Lbcm_loadtoc__dot_LC6_at_toc_at_ha
	ld 2, -24(1)
	add	20, 2, 3
	ld 3, -16(1)
	mtlr 3
	ld 3, -8(1)
	addi 1, 1, 288
# WAS ld 20,.LC6@toc@l(20)
	addi 1, 1, -288
	std 3, -8(1)
	mflr 3
	std 3, -16(1)
	std 2, -24(1)
	bl .Lbcm_loadtoc__dot_LC6_at_toc_at_l
	ld 2, -24(1)
	add 3, 3, 20
	ld 20, 0(3)
	ld 3, -16(1)
	mtlr 3
	ld 3, -8(1)
	addi 1, 1, 288
# WAS addi 25,25,.LC5@toc@l
	addi 1, 1, -288
	std 3, -8(1)
	mflr 3
	std 3, -16(1)
	std 2, -24(1)
	bl .Lbcm_loadtoc__dot_LC5_at_toc_at_l
	ld 2, -24(1)
	add	25, 25, 3
	ld 3, -16(1)
	mtlr 3
	ld 3, -8(1)
	addi 1, 1, 288
# WAS addi 26,26,.LC7@toc@l
	addi 1, 1, -288
	std 3, -8(1)
	mflr 3
	std 3, -16(1)
	std 2, -24(1)
	bl .Lbcm_loadtoc__dot_LC7_at_toc_at_l
	ld 2, -24(1)
	add	26, 26, 3
	ld 3, -16(1)
	mtlr 3
	ld 3, -8(1)
	addi 1, 1, 288
	addi 27,29,5
# WAS addi 28,28,.LC8@toc@l
	addi 1, 1, -288
	std 3, -8(1)
	mflr 3
	std 3, -16(1)
	std 2, -24(1)
	bl .Lbcm_loadtoc__dot_LC8_at_toc_at_l
	ld 2, -24(1)
	add	28, 28, 3
	ld 3, -16(1)
	mtlr 3
	ld 3, -8(1)
	addi 1, 1, 288
	addi 30,30,-29404
	.p2align 4,,15
.L2:

	ld 3,0(31)
	mr 5,21
	mr 6,29
	li 4,1
# WAS bl __fprintf_chk
	bl	bcm_redirector___fprintf_chk
	nop
	ld 3,0(31)
	mr 5,22
	mr 6,19
	li 4,1
# WAS bl __fprintf_chk
	bl	bcm_redirector___fprintf_chk
	nop
	ld 3,0(31)
	mr 5,23
	mr 6,24
	li 4,1
# WAS bl __fprintf_chk
	bl	bcm_redirector___fprintf_chk
	nop
	ld 3,0(31)
	mr 5,25
	mr 6,20
	li 4,1
# WAS bl __fprintf_chk
	bl	bcm_redirector___fprintf_chk
	nop
	ld 3,0(31)
	mr 5,26
	mr 6,27
	li 4,1
# WAS bl __fprintf_chk
	bl	bcm_redirector___fprintf_chk
	nop
	ld 3,0(31)
	li 4,1
	mr 5,28
	mr 6,30
# WAS bl __fprintf_chk
	bl	bcm_redirector___fprintf_chk
	nop
	b .L2
	.long 0
	.byte 0,0,0,1,128,13,0,0
	.size	exported_function,.-exported_function
	.section	".toc","aw"
	.set .LC11,.LC0
	.set .LC12,.LC3
	.set .LC13,.LC6
# WAS .section	".text"
.text
	.align 2
	.p2align 4,,15
	.type	function, @function
.Lfunction_local_target:
function:
0:
999:
	addis 2, 12, .LBORINGSSL_external_toc-999b@ha
	addi 2, 2, .LBORINGSSL_external_toc-999b@l
	ld 12, 0(2)
	add 2, 2, 12
# WAS addi 2,2,.TOC.-0b@l
	.localentry	function,.-function
.Lfunction_local_entry:
	mflr 0
	std 31,-8(1)
# WAS addis 31,2,.LC11@toc@ha		# gpr load fusion, type long
	addi 1, 1, -288
	std 3, -8(1)
	mflr 3
	std 3, -16(1)
	std 2, -24(1)
	bl .Lbcm_loadtoc__dot_LC11_at_toc_at_ha
	ld 2, -24(1)
	add	31, 2, 3
	ld 3, -16(1)
	mtlr 3
	ld 3, -8(1)
	addi 1, 1, 288
# WAS ld 31,.LC11@toc@l(31)
	addi 1, 1, -288
	std 3, -8(1)
	mflr 3
	std 3, -16(1)
	std 2, -24(1)
	bl .Lbcm_loadtoc__dot_LC11_at_toc_at_l
	ld 2, -24(1)
	add 3, 3, 31
	ld 31, 0(3)
	ld 3, -16(1)
	mtlr 3
	ld 3, -8(1)
	addi 1, 1, 288
# WAS addis 5,2,.LC1@toc@ha
	addi 1, 1, -288
	std 3, -8(1)
	mflr 3
	std 3, -16(1)
	std 2, -24(1)
	bl .Lbcm_loadtoc__dot_LC1_at_toc_at_ha
	ld 2, -24(1)
	add	5, 2, 3
	ld 3, -16(1)
	mtlr 3
	ld 3, -8(1)
	addi 1, 1, 288
	std 30,-16(1)
# WAS addis 30,2,.LANCHOR0@toc@ha
	addi 1, 1, -288
	std 3, -8(1)
	mflr 3
	std 3, -16(1)
	std 2, -24(1)
	bl .Lbcm_loadtoc__dot_LANCHOR0_at_toc_at_ha
	ld 2, -24(1)
	add	30, 2, 3
	ld 3, -16(1)
	mtlr 3
	ld 3, -8(1)
	addi 1, 1, 288
# WAS addi 5,5,.LC1@toc@l
	addi 1, 1, -288
	std 3, -8(1)
	mflr 3
	std 3, -16(1)
	std 2, -24(1)
	bl .Lbcm_loadtoc__dot_LC1_at_toc_at_l
	ld 2, -24(1)
	add	5, 5, 3
	ld 3, -16(1)
	mtlr 3
	ld 3, -8(1)
	addi 1, 1, 288
# WAS addi 30,30,.LANCHOR0@toc@l
	addi 1, 1, -288
	std 3, -8(1)
	mflr 3
	std 3, -16(1)
	std 2, -24(1)
	bl .Lbcm_loadtoc__dot_LANCHOR0_at_toc_at_l
	ld 2, -24(1)
	add	30, 30, 3
	ld 3, -16(1)
	mtlr 3
	ld 3, -8(1)
	addi 1, 1, 288
	li 4,1
	mr 6,30
	std 0,16(1)
	stdu 1,-112(1)
	ld 3,0(31)
# WAS bl __fprintf_chk
	bl	bcm_redirector___fprintf_chk
	nop
# WAS addis 6,2,.LC12@toc@ha		# gpr load fusion, type long
	addi 1, 1, -288
	std 3, -8(1)
	mflr 3
	std 3, -16(1)
	std 2, -24(1)
	bl .Lbcm_loadtoc__dot_LC12_at_toc_at_ha
	ld 2, -24(1)
	add	6, 2, 3
	ld 3, -16(1)
	mtlr 3
	ld 3, -8(1)
	addi 1, 1, 288
# WAS ld 6,.LC12@toc@l(6)
	addi 1, 1, -288
	std 3, -8(1)
	mflr 3
	std 3, -16(1)
	std 2, -24(1)
	bl .Lbcm_loadtoc__dot_LC12_at_toc_at_l
	ld 2, -24(1)
	add 3, 3, 6
	ld 6, 0(3)
	ld 3, -16(1)
	mtlr 3
	ld 3, -8(1)
	addi 1, 1, 288
	ld 3,0(31)
# WAS addis 5,2,.LC2@toc@ha
	addi 1, 1, -288
	std 3, -8(1)
	mflr 3
	std 3, -16(1)
	std 2, -24(1)
	bl .Lbcm_loadtoc__dot_LC2_at_toc_at_ha
	ld 2, -24(1)
	add	5, 2, 3
	ld 3, -16(1)
	mtlr 3
	ld 3, -8(1)
	addi 1, 1, 288
	li 4,1
# WAS addi 5,5,.LC2@toc@l
	addi 1, 1, -288
	std 3, -8(1)
	mflr 3
	std 3, -16(1)
	std 2, -24(1)
	bl .Lbcm_loadtoc__dot_LC2_at_toc_at_l
	ld 2, -24(1)
	add	5, 5, 3
	ld 3, -16(1)
	mtlr 3
	ld 3, -8(1)
	addi 1, 1, 288
# WAS bl __fprintf_chk
	bl	bcm_redirector___fprintf_chk
	nop
	ld 3,0(31)
# WAS addis 5,2,.LC4@toc@ha
	addi 1, 1, -288
	std 3, -8(1)
	mflr 3
	std 3, -16(1)
	std 2, -24(1)
	bl .Lbcm_loadtoc__dot_LC4_at_toc_at_ha
	ld 2, -24(1)
	add	5, 2, 3
	ld 3, -16(1)
	mtlr 3
	ld 3, -8(1)
	addi 1, 1, 288
# WAS addis 6,2,function@toc@ha
	addi 1, 1, -288
	std 3, -8(1)
	mflr 3
	std 3, -16(1)
	std 2, -24(1)
	bl .Lbcm_loadtoc__dot_Lfunction_local_target_at_toc_at_ha
	ld 2, -24(1)
	add	6, 2, 3
	ld 3, -16(1)
	mtlr 3
	ld 3, -8(1)
	addi 1, 1, 288
# WAS addi 5,5,.LC4@toc@l
	addi 1, 1, -288
	std 3, -8(1)
	mflr 3
	std 3, -16(1)
	std 2, -24(1)
	bl .Lbcm_loadtoc__dot_LC4_at_toc_at_l
	ld 2, -24(1)
	add	5, 5, 3
	ld 3, -16(1)
	mtlr 3
	ld 3, -8(1)
	addi 1, 1, 288
# WAS addi 6,6,function@toc@l
	addi 1, 1, -288
	std 3, -8(1)
	mflr 3
	std 3, -16(1)
	std 2, -24(1)
	bl .Lbcm_loadtoc__dot_Lfunction_local_target_at_toc_at_l
	ld 2, -24(1)
	add	6, 6, 3
	ld 3, -16(1)
	mtlr 3
	ld 3, -8(1)
	addi 1, 1, 288
	li 4,1
# WAS bl __fprintf_chk
	bl	bcm_redirector___fprintf_chk
	nop
# WAS addis 6,2,.LC13@toc@ha		# gpr load fusion, type long
	addi 1, 1, -288
	std 3, -8(1)
	mflr 3
	std 3, -16(1)
	std 2, -24(1)
	bl .Lbcm_loadtoc__dot_LC13_at_toc_at_ha
	ld 2, -24(1)
	add	6, 2, 3
	ld 3, -16(1)
	mtlr 3
	ld 3, -8(1)
	addi 1, 1, 288
# WAS ld 6,.LC13@toc@l(6)
	addi 1, 1, -288
	std 3, -8(1)
	mflr 3
	std 3, -16(1)
	std 2, -24(1)
	bl .Lbcm_loadtoc__dot_LC13_at_toc_at_l
	ld 2, -24(1)
	add 3, 3, 6
	ld 6, 0(3)
	ld 3, -16(1)
	mtlr 3
	ld 3, -8(1)
	addi 1, 1, 288
	ld 3,0(31)
# WAS addis 5,2,.LC5@toc@ha
	addi 1, 1, -288
	std 3, -8(1)
	mflr 3
	std 3, -16(1)
	std 2, -24(1)
	bl .Lbcm_loadtoc__dot_LC5_at_toc_at_ha
	ld 2, -24(1)
	add	5, 2, 3
	ld 3, -16(1)
	mtlr 3
	ld 3, -8(1)
	addi 1, 1, 288
	li 4,1
# WAS addi 5,5,.LC5@toc@l
	addi 1, 1, -288
	std 3, -8(1)
	mflr 3
	std 3, -16(1)
	std 2, -24(1)
	bl .Lbcm_loadtoc__dot_LC5_at_toc_at_l
	ld 2, -24(1)
	add	5, 5, 3
	ld 3, -16(1)
	mtlr 3
	ld 3, -8(1)
	addi 1, 1, 288
# WAS bl __fprintf_chk
	bl	bcm_redirector___fprintf_chk
	nop
	ld 3,0(31)
# WAS addis 5,2,.LC7@toc@ha
	addi 1, 1, -288
	std 3, -8(1)
	mflr 3
	std 3, -16(1)
	std 2, -24(1)
	bl .Lbcm_loadtoc__dot_LC7_at_toc_at_ha
	ld 2, -24(1)
	add	5, 2, 3
	ld 3, -16(1)
	mtlr 3
	ld 3, -8(1)
	addi 1, 1, 288
	addi 6,30,5
# WAS addi 5,5,.LC7@toc@l
	addi 1, 1, -288
	std 3, -8(1)
	mflr 3
	std 3, -16(1)
	std 2, -24(1)
	bl .Lbcm_loadtoc__dot_LC7_at_toc_at_l
	ld 2, -24(1)
	add	5, 5, 3
	ld 3, -16(1)
	mtlr 3
	ld 3, -8(1)
	addi 1, 1, 288
	li 4,1
# WAS bl __fprintf_chk
	bl	bcm_redirector___fprintf_chk
	nop
	ld 3,0(31)
	addis 6,30,0x5
# WAS addis 5,2,.LC8@toc@ha
	addi 1, 1, -288
	std 3, -8(1)
	mflr 3
	std 3, -16(1)
	std 2, -24(1)
	bl .Lbcm_loadtoc__dot_LC8_at_toc_at_ha
	ld 2, -24(1)
	add	5, 2, 3
	ld 3, -16(1)
	mtlr 3
	ld 3, -8(1)
	addi 1, 1, 288
	li 4,1
# WAS addi 5,5,.LC8@toc@l
	addi 1, 1, -288
	std 3, -8(1)
	mflr 3
	std 3, -16(1)
	std 2, -24(1)
	bl .Lbcm_loadtoc__dot_LC8_at_toc_at_l
	ld 2, -24(1)
	add	5, 5, 3
	ld 3, -16(1)
	mtlr 3
	ld 3, -8(1)
	addi 1, 1, 288
	addi 6,6,-29404
# WAS bl __fprintf_chk
	bl	bcm_redirector___fprintf_chk
	nop
# WAS bl exported_function
	bl	.Lexported_function_local_entry
	nop
	addi 1,1,112
	ld 0,16(1)
	ld 30,-16(1)
	ld 31,-8(1)
	mtlr 0
	blr
	.long 0
	.byte 0,0,0,1,128,2,0,0
	.size	function,.-function
	.globl kExportedString
# WAS .section	.rodata
.text
	.align 4
	.set	.LANCHOR0,. + 0
	.type	kString, @object
	.size	kString, 12
.LkString_local_target:
kString:
	.string	"hello world"
	.zero	4
	.type	kGiantArray, @object
	.size	kGiantArray, 400000
.LkGiantArray_local_target:
kGiantArray:
	.long	1
	.long	0
	.zero	399992
	.type	kExportedString, @object
	.size	kExportedString, 26
.LkExportedString_local_target:
kExportedString:
	.string	"hello world, more visibly"
# WAS .section	.rodata.str1.8,"aMS",@progbits,1
.text
	.align 3
.LC1:

	.string	"kString is %p\n"
	.zero	1
.LC2:

	.string	"kExportedString is %p\n"
	.zero	1
.LC4:

	.string	"function is %p\n"
.LC5:

	.string	"exported_function is %p\n"
	.zero	7
.LC7:

	.string	"&kString[5] is %p\n"
	.zero	5
.LC8:

	.string	"&kGiantArray[0x12345] is %p\n"
	.section	".bss"
	.align 2
	.type	bss, @object
	.size	bss, 20
bss:
.Lbss_local_target:

	.zero	20
	.ident	"GCC: (Ubuntu 4.9.2-10ubuntu13) 4.9.2"
	.section	.note.GNU-stack,"",@progbits
.text
BORINGSSL_bcm_text_end:
.type bcm_redirector___fprintf_chk, @function
bcm_redirector___fprintf_chk:
	mflr 0
	std 0,16(1)
	stdu 1,-32(1)
	bl	__fprintf_chk
	nop
	addi 1,1,32
	ld 0,16(1)
	mtlr 0
	blr
.type bss_bss_get, @function
bss_bss_get:
	addis 3, 2, .Lbss_local_target@toc@ha
	addi 3, 3, .Lbss_local_target@toc@l
	blr
.type bcm_loadtoc__dot_LANCHOR0_at_toc_at_ha, @function
bcm_loadtoc__dot_LANCHOR0_at_toc_at_ha:
.Lbcm_loadtoc__dot_LANCHOR0_at_toc_at_ha:
	addi 2, 0, 0
	addi 3, 0, 0
	addis 3, 2, .LANCHOR0@toc@ha
	blr
.type bcm_loadtoc__dot_LANCHOR0_at_toc_at_l, @function
bcm_loadtoc__dot_LANCHOR0_at_toc_at_l:
.Lbcm_loadtoc__dot_LANCHOR0_at_toc_at_l:
	addi 2, 0, 0
	addi 3, 2, .LANCHOR0@toc@l
	blr
.type bcm_loadtoc__dot_LC0_at_toc_at_ha, @function
bcm_loadtoc__dot_LC0_at_toc_at_ha:
.Lbcm_loadtoc__dot_LC0_at_toc_at_ha:
	addi 2, 0, 0
	addi 3, 0, 0
	addis 3, 2, .LC0@toc@ha
	blr
.type bcm_loadtoc__dot_LC0_at_toc_at_l, @function
bcm_loadtoc__dot_LC0_at_toc_at_l:
.Lbcm_loadtoc__dot_LC0_at_toc_at_l:
	addi 2, 0, 0
	addi 3, 2, .LC0@toc@l
	blr
.type bcm_loadtoc__dot_LC11_at_toc_at_ha, @function
bcm_loadtoc__dot_LC11_at_toc_at_ha:
.Lbcm_loadtoc__dot_LC11_at_toc_at_ha:
	addi 2, 0, 0
	addi 3, 0, 0
	addis 3, 2, .LC11@toc@ha
	blr
.type bcm_loadtoc__dot_LC11_at_toc_at_l, @function
bcm_loadtoc__dot_LC11_at_toc_at_l:
.Lbcm_loadtoc__dot_LC11_at_toc_at_l:
	addi 2, 0, 0
	addi 3, 2, .LC11@toc@l
	blr
.type bcm_loadtoc__dot_LC12_at_toc_at_ha, @function
bcm_loadtoc__dot_LC12_at_toc_at_ha:
.Lbcm_loadtoc__dot_LC12_at_toc_at_ha:
	addi 2, 0, 0
	addi 3, 0, 0
	addis 3, 2, .LC12@toc@ha
	blr
.type bcm_loadtoc__dot_LC12_at_toc_at_l, @function
bcm_loadtoc__dot_LC12_at_toc_at_l:
.Lbcm_loadtoc__dot_LC12_at_toc_at_l:
	addi 2, 0, 0
	addi 3, 2, .LC12@toc@l
	blr
.type bcm_loadtoc__dot_LC13_at_toc_at_ha, @function
bcm_loadtoc__dot_LC13_at_toc_at_ha:
.Lbcm_loadtoc__dot_LC13_at_toc_at_ha:
	addi 2, 0, 0
	addi 3, 0, 0
	addis 3, 2, .LC13@toc@ha
	blr
.type bcm_loadtoc__dot_LC13_at_toc_at_l, @function
bcm_loadtoc__dot_LC13_at_toc_at_l:
.Lbcm_loadtoc__dot_LC13_at_toc_at_l:
	addi 2, 0, 0
	addi 3, 2, .LC13@toc@l
	blr
.type bcm_loadtoc__dot_LC1_at_toc_at_ha, @function
bcm_loadtoc__dot_LC1_at_toc_at_ha:
.Lbcm_loadtoc__dot_LC1_at_toc_at_ha:
	addi 2, 0, 0
	addi 3, 0, 0
	addis 3, 2, .LC1@toc@ha
	blr
.type bcm_loadtoc__dot_LC1_at_toc_at_l, @function
bcm_loadtoc__dot_LC1_at_toc_at_l:
.Lbcm_loadtoc__dot_LC1_at_toc_at_l:
	addi 2, 0, 0
	addi 3, 2, .LC1@toc@l
	blr
.type bcm_loadtoc__dot_LC2_at_toc_at_ha, @function
bcm_loadtoc__dot_LC2_at_toc_at_ha:
.Lbcm_loadtoc__dot_LC2_at_toc_at_ha:
	addi 2, 0, 0
	addi 3, 0, 0
	addis 3, 2, .LC2@toc@ha
	blr
.type bcm_loadtoc__dot_LC2_at_toc_at_l, @function
bcm_loadtoc__dot_LC2_at_toc_at_l:
.Lbcm_loadtoc__dot_LC2_at_toc_at_l:
	addi 2, 0, 0
	addi 3, 2, .LC2@toc@l
	blr
.type bcm_loadtoc__dot_LC3_at_toc_at_ha, @function
bcm_loadtoc__dot_LC3_at_toc_at_ha:
.Lbcm_loadtoc__dot_LC3_at_toc_at_ha:
	addi 2, 0, 0
	addi 3, 0, 0
	addis 3, 2, .LC3@toc@ha
	blr
.type bcm_loadtoc__dot_LC3_at_toc_at_l, @function
bcm_loadtoc__dot_LC3_at_toc_at_l:
.Lbcm_loadtoc__dot_LC3_at_toc_at_l:
	addi 2, 0, 0
	addi 3, 2, .LC3@toc@l
	blr
.type bcm_loadtoc__dot_LC4_at_toc_at_ha, @function
bcm_loadtoc__dot_LC4_at_toc_at_ha:
.Lbcm_loadtoc__dot_LC4_at_toc_at_ha:
	addi 2, 0, 0
	addi 3, 0, 0
	addis 3, 2, .LC4@toc@ha
	blr
.type bcm_loadtoc__dot_LC4_at_toc_at_l, @function
bcm_loadtoc__dot_LC4_at_toc_at_l:
.Lbcm_loadtoc__dot_LC4_at_toc_at_l:
	addi 2, 0, 0
	addi 3, 2, .LC4@toc@l
	blr
.type bcm_loadtoc__dot_LC5_at_toc_at_ha, @function
bcm_loadtoc__dot_LC5_at_toc_at_ha:
.Lbcm_loadtoc__dot_LC5_at_toc_at_ha:
	addi 2, 0, 0
	addi 3, 0, 0
	addis 3, 2, .LC5@toc@ha
	blr
.type bcm_loadtoc__dot_LC5_at_toc_at_l, @function
bcm_loadtoc__dot_LC5_at_toc_at_l:
.Lbcm_loadtoc__dot_LC5_at_toc_at_l:
	addi 2, 0, 0
	addi 3, 2, .LC5@toc@l
	blr
.type bcm_loadtoc__dot_LC6_at_toc_at_ha, @function
bcm_loadtoc__dot_LC6_at_toc_at_ha:
.Lbcm_loadtoc__dot_LC6_at_toc_at_ha:
	addi 2, 0, 0
	addi 3, 0, 0
	addis 3, 2, .LC6@toc@ha
	blr
.type bcm_loadtoc__dot_LC6_at_toc_at_l, @function
bcm_loadtoc__dot_LC6_at_toc_at_l:
.Lbcm_loadtoc__dot_LC6_at_toc_at_l:
	addi 2, 0, 0
	addi 3, 2, .LC6@toc@l
	blr
.type bcm_loadtoc__dot_LC7_at_toc_at_ha, @function
bcm_loadtoc__dot_LC7_at_toc_at_ha:
.Lbcm_loadtoc__dot_LC7_at_toc_at_ha:
	addi 2, 0, 0
	addi 3, 0, 0
	addis 3, 2, .LC7@toc@ha
	blr
.type bcm_loadtoc__dot_LC7_at_toc_at_l, @function
bcm_loadtoc__dot_LC7_at_toc_at_l:
.Lbcm_loadtoc__dot_LC7_at_toc_at_l:
	addi 2, 0, 0
	addi 3, 2, .LC7@toc@l
	blr
.type bcm_loadtoc__dot_LC8_at_toc_at_ha, @function
bcm_loadtoc__dot_LC8_at_toc_at_ha:
.Lbcm_loadtoc__dot_LC8_at_toc_at_ha:
	addi 2, 0, 0
	addi 3, 0, 0
	addis 3, 2, .LC8@toc@ha
	blr
.type bcm_loadtoc__dot_LC8_at_toc_at_l, @function
bcm_loadtoc__dot_LC8_at_toc_at_l:
.Lbcm_loadtoc__dot_LC8_at_toc_at_l:
	addi 2, 0, 0
	addi 3, 2, .LC8@toc@l
	blr
.type bcm_loadtoc__dot_Lfunction_local_target_at_toc_at_ha, @function
bcm_loadtoc__dot_Lfunction_local_target_at_toc_at_ha:
.Lbcm_loadtoc__dot_Lfunction_local_target_at_toc_at_ha:
	addi 2, 0, 0
	addi 3, 0, 0
	addis 3, 2, .Lfunction_local_target@toc@ha
	blr
.type bcm_loadtoc__dot_Lfunction_local_target_at_toc_at_l, @function
bcm_loadtoc__dot_Lfunction_local_target_at_toc_at_l:
.Lbcm_loadtoc__dot_Lfunction_local_target_at_toc_at_l:
	addi 2, 0, 0
	addi 3, 2, .Lfunction_local_target@toc@l
	blr
.LBORINGSSL_external_toc:
.quad .TOC.-.LBORINGSSL_external_toc
.type BORINGSSL_bcm_text_hash, @object
.size BORINGSSL_bcm_text_hash, 64
BORINGSSL_bcm_text_hash:
.byte 0xae
.byte 0x2c
.byte 0xea
.byte 0x2a
.byte 0xbd
.byte 0xa6
.byte 0xf3
.byte 0xec
.byte 0x97
.byte 0x7f
.byte 0x9b
.byte 0xf6
.byte 0x94
.byte 0x9a
.byte 0xfc
.byte 0x83
.byte 0x68
.byte 0x27
.byte 0xcb
.byte 0xa0
.byte 0xa0
.byte 0x9f
.byte 0x6b
.byte 0x6f
.byte 0xde
.byte 0x52
.byte 0xcd
.byte 0xe2
.byte 0xcd
.byte 0xff
.byte 0x31
.byte 0x80
.byte 0xa2
.byte 0xd4
.byte 0xc3
.byte 0x66
.byte 0xf
.byte 0xc2
.byte 0x6a
.byte 0x7b
.byte 0xf4
.byte 0xbe
.byte 0x39
.byte 0xa2
.byte 0xd7
.byte 0x25
.byte 0xdb
.byte 0x21
.byte 0x98
.byte 0xe9
.byte 0xd5
.byte 0x53
.byte 0xbf
.byte 0x5c
.byte 0x32
.byte 0x6
.byte 0x83
.byte 0x34
.byte 0xc
.byte 0x65
.byte 0x89
.byte 0x52
.byte 0xbd
.byte 0x1f
