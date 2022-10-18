declare i8* @malloc(i64)
declare i32 @_setjmp(i8*) returns_twice
declare void @longjmp(i8*, i32) noreturn
declare i64 @jmpbuf_size()

declare [0 x {}]* @time()
declare void @print_time_diff([0 x {}]*, [0 x {}]*)
declare void @printInteger(i32)
declare void @printNL()


define i32 @main(i32 %_8310457, [0 x [0 x i8]*]* %_8310459) {
main_7708700:
    %_8310466.ret = call i32 @cc_main_7708709(i32 %_8310457, [0 x [0 x i8]*]* %_8310459)
    br label %_8310460

_8310460:
    %_8310542 = phi i32 [ %_8310466.ret, %main_7708700 ]
    ret i32 %_8310542

}

define i32 @cc_main_7708709(i32 %_8310596, [0 x [0 x i8]*]* %_8310601) {
cc_main_7708709:
    %_8009066.i8 = alloca i8, i64 400000
    %_8009066 = bitcast i8* %_8009066.i8 to [100000 x i32]*
    %_8009070.i8 = alloca i8, i64 400000
    %_8009070 = bitcast i8* %_8009070.i8 to [100000 x i32]*
    %_8009074.i8 = alloca i8, i64 400000
    %_8009074 = bitcast i8* %_8009074.i8 to [100000 x i32]*
    %_8009078.i8 = alloca i8, i64 400000
    %_8009078 = bitcast i8* %_8009078.i8 to [100000 x i32]*
    %_8009082.i8 = alloca i8, i64 400000
    %_8009082 = bitcast i8* %_8009082.i8 to [100000 x i32]*
    %_8009086.i8 = alloca i8, i64 400000
    %_8009086 = bitcast i8* %_8009086.i8 to [100000 x i32]*
    %_8009091.i8 = alloca i8, i64 1600000
    %_8009091 = bitcast i8* %_8009091.i8 to [100000 x {void ({}*, i32)*, {}*}]*
    %_8310391 = bitcast void ({[100000 x i32]*, [100000 x i32]*, i32, i32, i32}*, i32)* @mul_pb_8310165 to void ({}*, i32)*
    call void @init_7708746([100000 x i32]* %_8009066, i32 0)
    br label %init_a_8009112

init_a_8009112:
    call void @init_7708746([100000 x i32]* %_8009070, i32 1)
    br label %init_b_8009128

init_b_8009128:
    call void @const_8009135([100000 x i32]* %_8009074, i32 0)
    br label %init_c_8009235

init_c_8009235:
    call void @const_8009135([100000 x i32]* %_8009078, i32 0)
    br label %init_ad_8009255

init_ad_8009255:
    call void @const_8009135([100000 x i32]* %_8009082, i32 0)
    br label %init_bd_8009271

init_bd_8009271:
    call void @const_8009135([100000 x i32]* %_8009086, i32 1)
    br label %init_cd_8009287

init_cd_8009287:
    %_8310440.ret = call [0 x {}]* @time()
    br label %time_start_cont_8009345

time_start_cont_8009345:
    %start_time_8009426 = phi [0 x {}]* [ %_8310440.ret, %init_cd_8009287 ]
    %_8309857 = bitcast [100000 x {void ({}*, i32)*, {}*}]* %_8009091 to [0 x {void ({}*, i32)*, {}*}]*
    br label %loop_head_8009346

loop_head_8009346:
    %_8310003 = phi i32 [ 0, %time_start_cont_8009345 ], [ %_8310415, %enter_8309924 ]
    %_8310430 = icmp ult i32 %_8310003, 100000
    br i1 %_8310430, label %enter_8309924, label %exit_8009349

exit_8009349:
    br label %backward_loop_head_8009360

backward_loop_head_8009360:
    %_8309860 = phi i32 [ 0, %exit_8009349 ], [ %_8309899, %yield_8309887 ]
    %_8309916 = icmp ult i32 %_8309860, 100000
    br i1 %_8309916, label %enter_8009710, label %timer_8009367

timer_8009367:
    %_8009709.ret = call [0 x {}]* @time()
    br label %time_end_cont_8009379

time_end_cont_8009379:
    %end_time_8009438 = phi [0 x {}]* [ %_8009709.ret, %timer_8009367 ]
    call void @print_time_diff([0 x {}]* %start_time_8009426, [0 x {}]* %end_time_8009438)
    br label %print_ad_8009449

print_ad_8009449:
    call void @printArr_8009461([100000 x i32]* %_8009078)
    br label %print_bd_8009638

print_bd_8009638:
    call void @printArr_8009461([100000 x i32]* %_8009082)
    br label %print_cd_8009646

print_cd_8009646:
    call void @printArr_8009461([100000 x i32]* %_8009086)
    br label %print_a_8009654

print_a_8009654:
    call void @printArr_8009461([100000 x i32]* %_8009066)
    br label %print_b_8009662

print_b_8009662:
    call void @printArr_8009461([100000 x i32]* %_8009070)
    br label %print_c_8009670

print_c_8009670:
    call void @printArr_8009461([100000 x i32]* %_8009074)
    br label %callback_8009678

callback_8009678:
    br label %_8009683

_8009683:
    %_8311322 = phi i32 [ 1, %callback_8009678 ]
    ret i32 %_8311322

enter_8009710:
    %_8309870 = getelementptr inbounds [0 x {void ({}*, i32)*, {}*}], [0 x {void ({}*, i32)*, {}*}]* %_8309857, i64 0, i32 %_8309860
    %_8309878 = load {void ({}*, i32)*, {}*}, {void ({}*, i32)*, {}*}* %_8309870
    %_8309882 = extractvalue {void ({}*, i32)*, {}*} %_8309878, 0
    %_8309886 = extractvalue {void ({}*, i32)*, {}*} %_8309878, 1
    call void %_8309882({}* %_8309886, i32 1)
    br label %yield_8309887

yield_8309887:
    %_8309899 = add i32 1, %_8309860
    br label %backward_loop_head_8009360

enter_8309924:
    %_8310013 = getelementptr inbounds [100000 x i32], [100000 x i32]* %_8009066, i64 0, i32 %_8310003
    %_8310021 = load i32, i32* %_8310013
    %_8310040 = getelementptr inbounds [100000 x i32], [100000 x i32]* %_8009070, i64 0, i32 %_8310003
    %_8310048 = load i32, i32* %_8310040
    %_8310067 = getelementptr inbounds [100000 x i32], [100000 x i32]* %_8009074, i64 0, i32 %_8310003
    %_8310099 = mul i32 %_8310021, %_8310048
    store i32 %_8310099, i32* %_8310067
    ; %_8310109.i8 = call i8* @malloc(i64 32)
    %_8310109.i8 = alloca i8, i64 32
    %_8310109 = bitcast i8* %_8310109.i8 to {[100000 x i32]*, [100000 x i32]*, i32, i32, i32}*
    %_8310122.0 = insertvalue {[100000 x i32]*, [100000 x i32]*, i32, i32, i32} undef, [100000 x i32]* %_8009082, 0
    %_8310122.1 = insertvalue {[100000 x i32]*, [100000 x i32]*, i32, i32, i32} %_8310122.0, [100000 x i32]* %_8009078, 1
    %_8310122.2 = insertvalue {[100000 x i32]*, [100000 x i32]*, i32, i32, i32} %_8310122.1, i32 %_8310021, 2
    %_8310122.3 = insertvalue {[100000 x i32]*, [100000 x i32]*, i32, i32, i32} %_8310122.2, i32 %_8310003, 3
    %_8310122.4 = insertvalue {[100000 x i32]*, [100000 x i32]*, i32, i32, i32} %_8310122.3, i32 %_8310048, 4
    store {[100000 x i32]*, [100000 x i32]*, i32, i32, i32} %_8310122.4, {[100000 x i32]*, [100000 x i32]*, i32, i32, i32}* %_8310109
    %_8310145 = getelementptr inbounds [0 x {void ({}*, i32)*, {}*}], [0 x {void ({}*, i32)*, {}*}]* %_8309857, i64 0, i32 %_8310003
    %_8310397.0 = insertvalue {void ({}*, i32)*, {}*} undef, void ({}*, i32)* %_8310391, 0
    %_8310396 = bitcast {[100000 x i32]*, [100000 x i32]*, i32, i32, i32}* %_8310109 to {}*
    %_8310397.1 = insertvalue {void ({}*, i32)*, {}*} %_8310397.0, {}* %_8310396, 1
    store {void ({}*, i32)*, {}*} %_8310397.1, {void ({}*, i32)*, {}*}* %_8310145
    %_8310415 = add i32 1, %_8310003
    br label %loop_head_8009346

}

define void @const_8009135([100000 x i32]* %_8009163, i32 %_8009178) {
const_8009135:
    br label %loop_head_8009136

loop_head_8009136:
    %_8009166 = phi i32 [ 0, %const_8009135 ], [ %_8009196, %enter_8009146 ]
    %_8009211 = icmp ult i32 %_8009166, 100000
    br i1 %_8009211, label %enter_8009146, label %eta_br_8009137

enter_8009146:
    %_8009176 = getelementptr inbounds [100000 x i32], [100000 x i32]* %_8009163, i64 0, i32 %_8009166
    store i32 %_8009178, i32* %_8009176
    %_8009196 = add i32 1, %_8009166
    br label %loop_head_8009136

eta_br_8009137:
    br label %_8009138

_8009138:
    ret void

}

define void @init_7708746([100000 x i32]* %_7708888, i32 %_8008922) {
init_7708746:
    br label %loop_head_7708757

loop_head_7708757:
    %_7708891 = phi i32 [ 0, %init_7708746 ], [ %_8008952, %enter_7708787 ]
    %_8008982 = icmp ult i32 %_7708891, 100000
    br i1 %_8008982, label %enter_7708787, label %eta_br_7708765

eta_br_7708765:
    br label %_7708766

_7708766:
    ret void

enter_7708787:
    %_8008901 = getelementptr inbounds [100000 x i32], [100000 x i32]* %_7708888, i64 0, i32 %_7708891
    %_8008929 = add i32 %_7708891, %_8008922
    store i32 %_8008929, i32* %_8008901
    %_8008952 = add i32 1, %_7708891
    br label %loop_head_7708757

}

define void @mul_pb_8310165({[100000 x i32]*, [100000 x i32]*, i32, i32, i32}* %closure_env_8310236, i32 %_8310293) {
mul_pb_8310165:
    %_8310244 = load {[100000 x i32]*, [100000 x i32]*, i32, i32, i32}, {[100000 x i32]*, [100000 x i32]*, i32, i32, i32}* %closure_env_8310236
    %fv_ad_arr_8310259 = extractvalue {[100000 x i32]*, [100000 x i32]*, i32, i32, i32} %_8310244, 1
    %fv_i_8310263 = extractvalue {[100000 x i32]*, [100000 x i32]*, i32, i32, i32} %_8310244, 3
    %_8310273 = getelementptr inbounds [100000 x i32], [100000 x i32]* %fv_ad_arr_8310259, i64 0, i32 %fv_i_8310263
    %_8310281 = load i32, i32* %_8310273
    %fv_b_val_8310298 = extractvalue {[100000 x i32]*, [100000 x i32]*, i32, i32, i32} %_8310244, 4
    %_8310305 = mul i32 %_8310293, %fv_b_val_8310298
    %_8310316 = add i32 %_8310305, %_8310281
    store i32 %_8310316, i32* %_8310273
    %fv_bd_arr_8310334 = extractvalue {[100000 x i32]*, [100000 x i32]*, i32, i32, i32} %_8310244, 0
    %_8310344 = getelementptr inbounds [100000 x i32], [100000 x i32]* %fv_bd_arr_8310334, i64 0, i32 %fv_i_8310263
    %_8310352 = load i32, i32* %_8310344
    %fv_a_val_8310366 = extractvalue {[100000 x i32]*, [100000 x i32]*, i32, i32, i32} %_8310244, 2
    %_8310371 = mul i32 %_8310293, %fv_a_val_8310366
    %_8310382 = add i32 %_8310371, %_8310352
    store i32 %_8310382, i32* %_8310344
    br label %_8310170

_8310170:
    ret void

}

define void @printArr_8009461([100000 x i32]* %_8009552) {
printArr_8009461:
    br label %loop_head_8009462

loop_head_8009462:
    %_8009555 = phi i32 [ 0, %printArr_8009461 ], [ %_8009606, %yield_8009595 ]
    %_8009622 = icmp ult i32 %_8009555, 100000
    br i1 %_8009622, label %enter_8009488, label %exit_8009463

exit_8009463:
    call void @printNL()
    br label %_8009480

_8009480:
    ret void

enter_8009488:
    %_8009565 = getelementptr inbounds [100000 x i32], [100000 x i32]* %_8009552, i64 0, i32 %_8009555
    %_8009573 = load i32, i32* %_8009565
    call void @printInteger(i32 %_8009573)
    br label %yield_8009595

yield_8009595:
    %_8009606 = add i32 1, %_8009555
    br label %loop_head_8009462

}


