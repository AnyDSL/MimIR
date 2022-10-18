declare i8* @malloc(i64)
declare i32 @_setjmp(i8*) returns_twice
declare void @longjmp(i8*, i32) noreturn
declare i64 @jmpbuf_size()

declare [0 x {}]* @time()
declare void @print_time_diff([0 x {}]*, [0 x {}]*)
declare void @printNL()
declare void @printInteger(i32)


define i32 @main(i32 %_4101795, [0 x [0 x i8]*]* %_4101797) {
main_3799959:
    %_4101804.ret = call i32 @cc_main_3799968(i32 %_4101795, [0 x [0 x i8]*]* %_4101797)
    br label %_4101798

_4101798:
    %_4101880 = phi i32 [ %_4101804.ret, %main_3799959 ]
    ret i32 %_4101880

}

define i32 @cc_main_3799968(i32 %_4101934, [0 x [0 x i8]*]* %_4101939) {
cc_main_3799968:
    %_4100345.i8 = alloca i8, i64 400000
    %_4100345 = bitcast i8* %_4100345.i8 to [100000 x i32]*
    %_4100349.i8 = alloca i8, i64 400000
    %_4100349 = bitcast i8* %_4100349.i8 to [100000 x i32]*
    %_4100353.i8 = alloca i8, i64 400000
    %_4100353 = bitcast i8* %_4100353.i8 to [100000 x i32]*
    %_4100357.i8 = alloca i8, i64 400000
    %_4100357 = bitcast i8* %_4100357.i8 to [100000 x i32]*
    %_4100361.i8 = alloca i8, i64 400000
    %_4100361 = bitcast i8* %_4100361.i8 to [100000 x i32]*
    %_4100365.i8 = alloca i8, i64 400000
    %_4100365 = bitcast i8* %_4100365.i8 to [100000 x i32]*
    %_4100374.i8 = alloca i8, i64 32
    %_4100374 = bitcast i8* %_4100374.i8 to {void ({}*)*, {}*}*
    %_4100485 = bitcast {void ({}*)*, {}*}* %_4100374 to [0 x {void ({}*)*, {}*}]*
    %_4100495 = getelementptr inbounds [0 x {void ({}*)*, {}*}], [0 x {void ({}*)*, {}*}]* %_4100485, i64 0, i32 0
    %_4100525.0 = insertvalue {void ({}*)*, {}*} undef, void ({}*)* @end_4100510, 0
    %_4100525.1 = insertvalue {void ({}*)*, {}*} %_4100525.0, {}* undef, 1
    store {void ({}*)*, {}*} %_4100525.1, {void ({}*)*, {}*}* %_4100495
    %_4101729 = bitcast void ({[100000 x i32]*, {void ({}*)*, {}*}, i32, i32, [100000 x i32]*, i32, [100000 x i32]*}*)* @pb_4101427 to void ({}*)*
    call void @init_3800006([100000 x i32]* %_4100345, i32 0)
    br label %init_a_4100551

init_a_4100551:
    call void @init_3800006([100000 x i32]* %_4100349, i32 1)
    br label %init_b_4100567

init_b_4100567:
    call void @const_4100574([100000 x i32]* %_4100353, i32 0)
    br label %init_c_4100673

init_c_4100673:
    call void @const_4100574([100000 x i32]* %_4100357, i32 0)
    br label %init_ad_4100689

init_ad_4100689:
    call void @const_4100574([100000 x i32]* %_4100361, i32 0)
    br label %init_bd_4100705

init_bd_4100705:
    call void @const_4100574([100000 x i32]* %_4100365, i32 1)
    br label %init_cd_4100721

init_cd_4100721:
    %_4101778.ret = call [0 x {}]* @time()
    br label %time_start_cont_4100779

time_start_cont_4100779:
    %start_time_4100901 = phi [0 x {}]* [ %_4101778.ret, %init_cd_4100721 ]
    br label %loop_head_4100780

loop_head_4100780:
    %_4101261 = phi i32 [ 0, %time_start_cont_4100779 ], [ %_4101753, %enter_4101190 ]
    %_4101768 = icmp ult i32 %_4101261, 100000
    br i1 %_4101768, label %enter_4101190, label %exit_4100783

enter_4101190:
    %_4101271 = getelementptr inbounds [100000 x i32], [100000 x i32]* %_4100345, i64 0, i32 %_4101261
    %_4101279 = load i32, i32* %_4101271
    %_4101298 = getelementptr inbounds [100000 x i32], [100000 x i32]* %_4100349, i64 0, i32 %_4101261
    %_4101306 = load i32, i32* %_4101298
    %_4101325 = getelementptr inbounds [100000 x i32], [100000 x i32]* %_4100353, i64 0, i32 %_4101261
    %_4101357 = mul i32 %_4101279, %_4101306
    store i32 %_4101357, i32* %_4101325
    %_4101372 = load {void ({}*)*, {}*}, {void ({}*)*, {}*}* %_4100495
    %_4101377.i8 = call i8* @malloc(i64 56)
    %_4101377 = bitcast i8* %_4101377.i8 to {[100000 x i32]*, {void ({}*)*, {}*}, i32, i32, [100000 x i32]*, i32, [100000 x i32]*}*
    %_4101396.0 = insertvalue {[100000 x i32]*, {void ({}*)*, {}*}, i32, i32, [100000 x i32]*, i32, [100000 x i32]*} undef, [100000 x i32]* %_4100361, 0
    %_4101396.1 = insertvalue {[100000 x i32]*, {void ({}*)*, {}*}, i32, i32, [100000 x i32]*, i32, [100000 x i32]*} %_4101396.0, {void ({}*)*, {}*} %_4101372, 1
    %_4101396.2 = insertvalue {[100000 x i32]*, {void ({}*)*, {}*}, i32, i32, [100000 x i32]*, i32, [100000 x i32]*} %_4101396.1, i32 %_4101261, 2
    %_4101396.3 = insertvalue {[100000 x i32]*, {void ({}*)*, {}*}, i32, i32, [100000 x i32]*, i32, [100000 x i32]*} %_4101396.2, i32 %_4101279, 3
    %_4101396.4 = insertvalue {[100000 x i32]*, {void ({}*)*, {}*}, i32, i32, [100000 x i32]*, i32, [100000 x i32]*} %_4101396.3, [100000 x i32]* %_4100357, 4
    %_4101396.5 = insertvalue {[100000 x i32]*, {void ({}*)*, {}*}, i32, i32, [100000 x i32]*, i32, [100000 x i32]*} %_4101396.4, i32 %_4101306, 5
    %_4101396.6 = insertvalue {[100000 x i32]*, {void ({}*)*, {}*}, i32, i32, [100000 x i32]*, i32, [100000 x i32]*} %_4101396.5, [100000 x i32]* %_4100365, 6
    store {[100000 x i32]*, {void ({}*)*, {}*}, i32, i32, [100000 x i32]*, i32, [100000 x i32]*} %_4101396.6, {[100000 x i32]*, {void ({}*)*, {}*}, i32, i32, [100000 x i32]*, i32, [100000 x i32]*}* %_4101377
    %_4101735.0 = insertvalue {void ({}*)*, {}*} undef, void ({}*)* %_4101729, 0
    %_4101734 = bitcast {[100000 x i32]*, {void ({}*)*, {}*}, i32, i32, [100000 x i32]*, i32, [100000 x i32]*}* %_4101377 to {}*
    %_4101735.1 = insertvalue {void ({}*)*, {}*} %_4101735.0, {}* %_4101734, 1
    store {void ({}*)*, {}*} %_4101735.1, {void ({}*)*, {}*}* %_4100495
    %_4101753 = add i32 1, %_4101261
    br label %loop_head_4100780

exit_4100783:
    %_4100827 = load {void ({}*)*, {}*}, {void ({}*)*, {}*}* %_4100495
    %_4100831 = extractvalue {void ({}*)*, {}*} %_4100827, 0
    %_4100835 = extractvalue {void ({}*)*, {}*} %_4100827, 1
    call void %_4100831({}* %_4100835)
    br label %timer_4100842

timer_4100842:
    %_4101187.ret = call [0 x {}]* @time()
    br label %time_end_cont_4100854

time_end_cont_4100854:
    %end_time_4100913 = phi [0 x {}]* [ %_4101187.ret, %timer_4100842 ]
    call void @print_time_diff([0 x {}]* %start_time_4100901, [0 x {}]* %end_time_4100913)
    br label %print_ad_4100924

print_ad_4100924:
    call void @printArr_4100936([100000 x i32]* %_4100357)
    br label %print_bd_4101117

print_bd_4101117:
    call void @printArr_4100936([100000 x i32]* %_4100361)
    br label %print_cd_4101125

print_cd_4101125:
    call void @printArr_4100936([100000 x i32]* %_4100365)
    br label %print_a_4101133

print_a_4101133:
    call void @printArr_4100936([100000 x i32]* %_4100345)
    br label %print_b_4101141

print_b_4101141:
    call void @printArr_4100936([100000 x i32]* %_4100349)
    br label %print_c_4101149

print_c_4101149:
    call void @printArr_4100936([100000 x i32]* %_4100353)
    br label %callback_4101156

callback_4101156:
    br label %_4101161

_4101161:
    %_4102780 = phi i32 [ 1, %callback_4101156 ]
    ret i32 %_4102780

}

define void @pb_4101427({[100000 x i32]*, {void ({}*)*, {}*}, i32, i32, [100000 x i32]*, i32, [100000 x i32]*}* %closure_env_4101458) {
pb_4101427:
    %_4101466 = load {[100000 x i32]*, {void ({}*)*, {}*}, i32, i32, [100000 x i32]*, i32, [100000 x i32]*}, {[100000 x i32]*, {void ({}*)*, {}*}, i32, i32, [100000 x i32]*, i32, [100000 x i32]*}* %closure_env_4101458
    %fv_last_pb_4101472 = extractvalue {[100000 x i32]*, {void ({}*)*, {}*}, i32, i32, [100000 x i32]*, i32, [100000 x i32]*} %_4101466, 1
    %_4101474 = extractvalue {void ({}*)*, {}*} %fv_last_pb_4101472, 0
    %fv_cd_arr_4101531 = extractvalue {[100000 x i32]*, {void ({}*)*, {}*}, i32, i32, [100000 x i32]*, i32, [100000 x i32]*} %_4101466, 6
    %fv_i_4101536 = extractvalue {[100000 x i32]*, {void ({}*)*, {}*}, i32, i32, [100000 x i32]*, i32, [100000 x i32]*} %_4101466, 2
    %_4101546 = getelementptr inbounds [100000 x i32], [100000 x i32]* %fv_cd_arr_4101531, i64 0, i32 %fv_i_4101536
    %_4101554 = load i32, i32* %_4101546
    store i32 0, i32* %_4101546
    %fv_ad_arr_4101575 = extractvalue {[100000 x i32]*, {void ({}*)*, {}*}, i32, i32, [100000 x i32]*, i32, [100000 x i32]*} %_4101466, 4
    %_4101585 = getelementptr inbounds [100000 x i32], [100000 x i32]* %fv_ad_arr_4101575, i64 0, i32 %fv_i_4101536
    %_4101593 = load i32, i32* %_4101585
    %fv_b_val_4101619 = extractvalue {[100000 x i32]*, {void ({}*)*, {}*}, i32, i32, [100000 x i32]*, i32, [100000 x i32]*} %_4101466, 5
    %_4101634 = mul i32 %fv_b_val_4101619, %_4101554
    %_4101639 = add i32 %_4101593, %_4101634
    store i32 %_4101639, i32* %_4101585
    %fv_bd_arr_4101658 = extractvalue {[100000 x i32]*, {void ({}*)*, {}*}, i32, i32, [100000 x i32]*, i32, [100000 x i32]*} %_4101466, 0
    %_4101668 = getelementptr inbounds [100000 x i32], [100000 x i32]* %fv_bd_arr_4101658, i64 0, i32 %fv_i_4101536
    %_4101676 = load i32, i32* %_4101668
    %fv_a_val_4101702 = extractvalue {[100000 x i32]*, {void ({}*)*, {}*}, i32, i32, [100000 x i32]*, i32, [100000 x i32]*} %_4101466, 3
    %_4101707 = mul i32 %_4101554, %fv_a_val_4101702
    %_4101712 = add i32 %_4101676, %_4101707
    store i32 %_4101712, i32* %_4101668
    %_4101721 = extractvalue {void ({}*)*, {}*} %fv_last_pb_4101472, 1
    call void %_4101474({}* %_4101721)
    br label %_4101722

_4101722:
    ret void

}

define void @const_4100574([100000 x i32]* %_4100602, i32 %_4100617) {
const_4100574:
    br label %loop_head_4100575

loop_head_4100575:
    %_4100605 = phi i32 [ 0, %const_4100574 ], [ %_4100635, %enter_4100585 ]
    %_4100650 = icmp ult i32 %_4100605, 100000
    br i1 %_4100650, label %enter_4100585, label %eta_br_4100576

enter_4100585:
    %_4100615 = getelementptr inbounds [100000 x i32], [100000 x i32]* %_4100602, i64 0, i32 %_4100605
    store i32 %_4100617, i32* %_4100615
    %_4100635 = add i32 1, %_4100605
    br label %loop_head_4100575

eta_br_4100576:
    br label %_4100577

_4100577:
    ret void

}

define void @printArr_4100936([100000 x i32]* %_4101031) {
printArr_4100936:
    br label %loop_head_4100937

loop_head_4100937:
    %_4101034 = phi i32 [ 0, %printArr_4100936 ], [ %_4101085, %yield_4101074 ]
    %_4101101 = icmp ult i32 %_4101034, 100000
    br i1 %_4101101, label %enter_4100967, label %exit_4100938

exit_4100938:
    call void @printNL()
    br label %_4100959

_4100959:
    ret void

enter_4100967:
    %_4101044 = getelementptr inbounds [100000 x i32], [100000 x i32]* %_4101031, i64 0, i32 %_4101034
    %_4101052 = load i32, i32* %_4101044
    call void @printInteger(i32 %_4101052)
    br label %yield_4101074

yield_4101074:
    %_4101085 = add i32 1, %_4101034
    br label %loop_head_4100937

}

define void @init_3800006([100000 x i32]* %_3800147, i32 %_4100181) {
init_3800006:
    br label %loop_head_3800017

loop_head_3800017:
    %_3800150 = phi i32 [ 0, %init_3800006 ], [ %_4100211, %enter_3800047 ]
    %_4100240 = icmp ult i32 %_3800150, 100000
    br i1 %_4100240, label %enter_3800047, label %eta_br_3800025

eta_br_3800025:
    br label %_3800026

_3800026:
    ret void

enter_3800047:
    %_4100160 = getelementptr inbounds [100000 x i32], [100000 x i32]* %_3800147, i64 0, i32 %_3800150
    %_4100188 = add i32 %_3800150, %_4100181
    store i32 %_4100188, i32* %_4100160
    %_4100211 = add i32 1, %_3800150
    br label %loop_head_3800017

}

define void @end_4100510({}* %_4103518) {
end_4100510:
    br label %_4100515

_4100515:
    ret void

}


