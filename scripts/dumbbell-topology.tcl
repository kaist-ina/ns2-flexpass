set ns [new Simulator]

# Configurations
set N 10
set flowPerHost 2
set K 65
set ALPHA 0.5
set w_init 0.5
set B_host 1000;# Host buffer packets
set linkBW 10Gb
set inputlinkBW 10Gb
set linkLatency 10us
set linkLatency_sec 0.00001

set packetSize 1460
set creditQueueCapacity [expr 84*2]  ;# bytes
set dataQueueCapacity [expr 250*1538]; 
set hostQueueCapacity [expr 1538*1000] ;# bytes
set maxCrditBurst [expr 84*2] ;# bytes
set creditRate [expr 64734895] ;# bytes / sec
set creditRate2 [expr 64734895] ;# bytes / sec
set baseCreditRate $creditRate
set interFlowDelay 0.0000 ;# secs
set expID [expr int([lindex $argv 1])]
set K_port $K;	#The per-port ECN marking threshold
set K_xpass 25;	# XPass queue ECN marking threshold (for MQ-GDX)

set egdxSelDropThresh 200000
set egdxScheme 2
set enableSharedBuffer 1
set newEgdxAllocationLogic 1
set reordering_measure_in_rc3_ 0; #only for stat purpose. must be 0 normally
set static_allocation_ 0; #only for stat purpose. must be 0 normally


set sender_type "both"; #both, new, legacy
# set sender_type "legacy"; #both, new, legacy
# set sender_type "new";

if  {$enableSharedBuffer} {
    set dataQueueCapacity  [expr 9999*1000*1000] ; # set to infinity. RED queue itself should not drop any packets!
}
if {!$newEgdxAllocationLogic} {
  puts "Caution: Using legacy allocation logic!"
}

# MQ-GDX and E-GDX
# xpass_queue_weight: Configurable parameter of the weight of XPass queue, assuming there is no credit queue. Valid value: (0, 1)
# This value should be applied to DWRR queues (DWRR::set-quantum) (since they don't consider credit queue when doing DWRR)
set xpass_queue_weight 0.5
set queue_quantum_default 1538
set actual_xpass_queue_weight 0.5 ;# will be calculated later

set ccc_type [lindex $argv 0]; # xpass, gdx, mqgdx, egdx, negdx

if {([string compare $ccc_type "xpass"] == 0)} {
    # set enable_dwrr_q 0;
    set enable_dwrr_q 1;
    set enable_tos_ 0;
    set enable_non_xpass_selective_dropping_ 0;
} elseif {([string compare $ccc_type "gdx"] == 0)} {
    set enable_dwrr_q 1;
    set enable_tos_ 0;
    set enable_non_xpass_selective_dropping_ 0;
} elseif {([string compare $ccc_type "mqgdx"] == 0) || ([string compare $ccc_type "egdx"] == 0)} {
    set enable_dwrr_q 1;
    set enable_tos_ 1;
    set enable_non_xpass_selective_dropping_ 0;
} elseif {([string compare $ccc_type "negdx"] == 0)} {
    set enable_dwrr_q 1;
    set enable_tos_ 1;
    set enable_non_xpass_selective_dropping_ 1;
} else {
    puts "Invalid Credit-based CC type: must be one of xpass, gdx, mqgdx, egdx, negdx"
    exit 1
}

if {[string compare $ccc_type "egdx"] == 0} {
    # xpass_queue_weight_ratio: ratio of XPass queue weight compared to TCP queue weight
    set xpass_queue_weight_ratio [expr $xpass_queue_weight / (1 - $xpass_queue_weight)] 
    # actual_xpass_queue_weight: actual weight of XPass queue, considering credit queue
    # This value should be applied to credit generators (Agents::egdx_beta_) and credit rate limiters (DWRR::token_refresh_rate_)
    set actual_xpass_queue_weight [expr 1538 / (1538 + (84+1538) * $xpass_queue_weight_ratio)];
    
    puts "xpass_queue_weight = ${xpass_queue_weight}"
    puts "actual_xpass_queue_weight = ${actual_xpass_queue_weight}"
} elseif {([string compare $ccc_type "negdx"] == 0)} {
    # set xpass_queue_weight_ratio 0;
    # set actual_xpass_queue_weight 0.5;
    
    # xpass_queue_weight_ratio: ratio of XPass queue weight compared to TCP queue weight
    # set xpass_queue_weight_ratio [expr $xpass_queue_weight / (1 - $xpass_queue_weight)] 
    # actual_xpass_queue_weight: actual weight of XPass queue, considering credit queue
    # This value should be applied to credit generators (Agents::egdx_beta_) and credit rate limiters (DWRR::token_refresh_rate_)
    set actual_xpass_queue_weight [expr (1538.0-84.0)/ 1538 * $xpass_queue_weight];
    
    puts "xpass_queue_weight = ${xpass_queue_weight}"
    puts "actual_xpass_queue_weight = ${actual_xpass_queue_weight}"
    
    set creditRate [expr $creditRate * $actual_xpass_queue_weight];
    set creditRate2 [expr $creditRate2 * $actual_xpass_queue_weight];
    set baseCreditRate [expr $baseCreditRate * $actual_xpass_queue_weight];
}

puts "Base Credit Rate = $baseCreditRate"

# Output file
file mkdir "outputs"
set nt [open outputs/trace_$expID.out w]
set fct_out [open outputs/fct_$expID.out w]

set tot_qlenfile [open outputs/tot_qlenfile_$expID.tr w]
set qlenfile [open outputs/qlenfile_$expID.tr w]

puts $fct_out "Flow ID,Flow Size (bytes),Flow Completion Time (secs)"
close $fct_out


set vt [open outputs/var_$expID.out w]


proc finish {} {
    global ns nt vt sender N
    $ns flush-trace 
    for {set i 1} {$i < $N} {incr i} {
    # $sender($i) flush-trace
    }
    close $nt
    close $vt
    puts "Simulation terminated successfully."
    exit 0
}
$ns trace-all $nt

Queue/BroadcomNode set selective_dropping_threshold_ $egdxSelDropThresh

puts "Creating Nodes..."
set left_gateway [$ns node]
set right_gateway [$ns node]
set left_bcm [$ns broadcom]
set right_bcm [$ns broadcom]

for {set i 0} {$i < $N} {incr i} {
  set left_node($i) [$ns node]
}

for {set i 0} {$i < $N} {incr i} {
  set right_node($i) [$ns node]
}

Queue/DropTail set mean_pktsize_ 1538
Queue/DropTail set limit_ $B_host

#Credit Setting
Queue/XPassDropTail set credit_limit_ $creditQueueCapacity
Queue/XPassDropTail set data_limit_ $dataQueueCapacity
Queue/XPassDropTail set token_refresh_rate_ $creditRate

#RED Setting
if {$enable_dwrr_q} {
    Queue/DwrrXPassRED set queue_num_ 3
    Queue/DwrrXPassRED set mean_pktsize_ 1538
    
    Queue/DwrrXPassRED set thresh_ $K_port
    Queue/DwrrXPassRED set maxthresh_ $K_port
    Queue/DwrrXPassRED set mark_p_ 2.0
    Queue/DwrrXPassRED set port_thresh_ $K_port
    
    Queue/DwrrXPassRED set marking_scheme_ 0; # Use per-queue ECN
    Queue/DwrrXPassRED set estimate_round_alpha_ 0.75
    Queue/DwrrXPassRED set estimate_round_idle_interval_bytes_ 1500
    
    
    Queue/DwrrXPassRED set link_capacity_ 10Gb
    Queue/DwrrXPassRED set deque_marking_ false
    Queue/DwrrXPassRED set debug_ false
    Queue/DwrrXPassRED set credit_limit_ $creditQueueCapacity
    Queue/DwrrXPassRED set data_limit_ $dataQueueCapacity
    Queue/DwrrXPassRED set queue_in_bytes_ true;
    Queue/DwrrXPassRED set limit_ [expr $dataQueueCapacity / 1538];

    
    if {([string compare $ccc_type "egdx"] == 0) || ([string compare $ccc_type "negdx"] == 0)} {
        Queue/DwrrXPassRED set token_refresh_rate_ [expr $creditRate * $actual_xpass_queue_weight]
    } else {
        Queue/DwrrXPassRED set token_refresh_rate_ [expr $creditRate]
    }
    Queue/DwrrXPassRED set max_tokens_ [expr 2*84] 
    Queue/DwrrXPassRED set exp_id_ $expID
    Queue/DwrrXPassRED set qidx_ 1
    
    Queue/DwrrXPassRED set bytes_ false
    Queue/DwrrXPassRED set setbit_ true
    Queue/DwrrXPassRED set gentle_ false
    Queue/DwrrXPassRED set q_weight_ 1.0

    Queue/DwrrXPassRED set enable_tos_ $enable_tos_
    Queue/DwrrXPassRED set enable_non_xpass_selective_dropping_ $enable_non_xpass_selective_dropping_
    
    Queue/DwrrXPassRED set enable_shared_buffer_ $enableSharedBuffer
    Queue/DwrrXPassRED set selective_dropping_threshold_ $egdxSelDropThresh
    Queue/DwrrXPassRED set egdx_queue_scheme_ $egdxScheme

} else {
    Queue/XPassRED set queue_num_ 3
    Queue/XPassRED set mean_pktsize_ 1538
    
    Queue/XPassRED set thresh_ $K_port
    Queue/XPassRED set maxthresh_ $K_port
    Queue/XPassRED set mark_p_ 2.0
    Queue/XPassRED set port_thresh_ $K_port

    Queue/XPassRED set marking_scheme_ 0; # Use per-queue ECN
    Queue/XPassRED set estimate_round_alpha_ 0.75
    Queue/XPassRED set estimate_round_idle_interval_bytes_ 1500

    Queue/XPassRED set link_capacity_ 10Gb
    Queue/XPassRED set deque_marking_ false
    Queue/XPassRED set debug_ false
    Queue/XPassRED set credit_limit_ $creditQueueCapacity
    Queue/XPassRED set data_limit_ $dataQueueCapacity; # no use
    Queue/XPassRED set queue_in_bytes_ true;
    Queue/XPassRED set limit_ [expr $dataQueueCapacity / 1538];

    Queue/XPassRED set token_refresh_rate_ [expr $creditRate]
    Queue/XPassRED set max_tokens_ [expr 2*84] 
    Queue/XPassRED set exp_id_ $expID
    Queue/XPassRED set qidx_ 1

    Queue/XPassRED set bytes_ false
    Queue/XPassRED set setbit_ true
    Queue/XPassRED set gentle_ false
    Queue/XPassRED set q_weight_ 1.0
}

# Why?

puts "Creating Links..."

for {set i 0} {$i < $N} {incr i} {
#   $ns simplex-link $left_node($i) $left_gateway $inputlinkBW $linkLatency DropTail
#   $ns simplex-link $left_gateway $left_node($i) $inputlinkBW $linkLatency DropTail; # DwrrXPassRED

#   $ns simplex-link $right_node($i) $right_gateway $inputlinkBW $linkLatency DropTail; # DwrrXPassRED
#   $ns simplex-link $right_gateway $right_node($i) $inputlinkBW $linkLatency DropTail
    if {$enable_dwrr_q} {
        $ns simplex-link $left_node($i) $left_gateway $inputlinkBW $linkLatency DwrrXPassRED
        set L [$ns link $left_node($i) $left_gateway]
        set q [$L set queue_]
        $q set-quantum 0 $queue_quantum_default
        $q set-quantum 1 [expr $queue_quantum_default*2*$xpass_queue_weight]
        $q set-quantum 2 [expr $queue_quantum_default*2*(1-$xpass_queue_weight)]
        $q set-thresh 1 [expr $K_xpass]
        $q set-thresh 2 $K_port
        $q attach-total $tot_qlenfile
        $q attach-queue $qlenfile
        $q ingress-node $left_bcm
        $L set trace_ 1
        $L set qidx_ 1
        
        $ns simplex-link $left_gateway $left_node($i) $inputlinkBW $linkLatency DwrrXPassRED
        set L [$ns link $left_gateway $left_node($i)]
        set q [$L set queue_]
        $q set-quantum 0 $queue_quantum_default
        $q set-quantum 1 [expr $queue_quantum_default*2*$xpass_queue_weight]
        $q set-quantum 2 [expr $queue_quantum_default*2*(1-$xpass_queue_weight)]
        $q set-thresh 1 [expr $K_xpass]
        $q set-thresh 2 $K_port
        $q attach-total $tot_qlenfile
        $q attach-queue $qlenfile
        $q egress-node $left_bcm
        $L set trace_ 1
        $L set qidx_ 1
        
        $ns simplex-link $right_node($i) $right_gateway $inputlinkBW $linkLatency DwrrXPassRED
        set L [$ns link $right_node($i) $right_gateway]
        set q [$L set queue_]
        $q set-quantum 0 $queue_quantum_default
        $q set-quantum 1 [expr $queue_quantum_default*2*$xpass_queue_weight]
        $q set-quantum 2 [expr $queue_quantum_default*2*(1-$xpass_queue_weight)]
        $q set-thresh 1 [expr $K_xpass]
        $q set-thresh 2 $K_port
        $q attach-total $tot_qlenfile
        $q attach-queue $qlenfile
        $q ingress-node $right_bcm
        $L set trace_ 1
        $L set qidx_ 1

        $ns simplex-link $right_gateway $right_node($i) $inputlinkBW $linkLatency DwrrXPassRED
        set L [$ns link $right_gateway $right_node($i)]
        set q [$L set queue_]
        $q set-quantum 0 $queue_quantum_default
        $q set-quantum 1 [expr $queue_quantum_default*2*$xpass_queue_weight]
        $q set-quantum 2 [expr $queue_quantum_default*2*(1-$xpass_queue_weight)]
        $q set-thresh 1 [expr $K_xpass]
        $q set-thresh 2 $K_port
        $q attach-total $tot_qlenfile
        $q attach-queue $qlenfile
        $q egress-node $right_bcm
        $L set trace_ 1
        $L set qidx_ 1
    } else {
        $ns simplex-link $left_node($i) $left_gateway $inputlinkBW $linkLatency XPassRED;
        $ns simplex-link $left_gateway $left_node($i) $inputlinkBW $linkLatency XPassRED;
        $ns simplex-link $right_node($i) $right_gateway $inputlinkBW $linkLatency XPassRED;
        $ns simplex-link $right_gateway $right_node($i) $inputlinkBW $linkLatency XPassRED;
    }
}
if {$enable_dwrr_q} {
    $ns duplex-link $left_gateway $right_gateway $linkBW $linkLatency DwrrXPassRED
    set L [$ns link $left_gateway $right_gateway]
    set q [$L set queue_]
    $q set-quantum 0 $queue_quantum_default
    $q set-quantum 1 [expr $queue_quantum_default*2*$xpass_queue_weight]
    $q set-quantum 2 [expr $queue_quantum_default*2*(1-$xpass_queue_weight)]
    $q set-thresh 1 [expr $K_xpass]
    $q set-thresh 2 $K_port
    $q attach-total $tot_qlenfile
    $q attach-queue $qlenfile
    $q ingress-node $right_bcm
    $q egress-node $left_bcm
    $L set trace_ 1
    $L set qidx_ 1
    set bottleneck_queue_lr $q
    
    set L [$ns link $right_gateway $left_gateway]
    set q [$L set queue_]
    $q set-quantum 0 $queue_quantum_default
    $q set-quantum 1 [expr $queue_quantum_default*2*$xpass_queue_weight]
    $q set-quantum 2 [expr $queue_quantum_default*2*(1-$xpass_queue_weight)]
    $q set-thresh 1 [expr $K_xpass]
    $q set-thresh 2 $K_port
    $q ingress-node $left_bcm
    $q egress-node $right_bcm
    $L set trace_ 0
    $L set qidx_ 2
    set bottleneck_queue_rl $q

} else {
    $ns duplex-link $left_gateway $right_gateway $linkBW $linkLatency XPassRED
    set L [$ns link $left_gateway $right_gateway]
    set q [$L set queue_]
    $q attach-total $tot_qlenfile
    $q attach-queue $qlenfile
}


puts "Creating Agents..."

Agent/TCP set ecn_ 1
Agent/TCP set old_ecn_ 1
Agent/TCP set packetSize_ 1454
Agent/TCP set window_ 180
Agent/TCP set slow_start_restart_ false
Agent/TCP set tcpTick_ 0.000001
Agent/TCP set minrto_ 0.004
Agent/TCP set rtxcur_init_ 0.001
Agent/TCP set windowOption_ 0
Agent/TCP set tcpip_base_hdr_size_ 84

Agent/TCP set dctcp_ true
Agent/TCP set dctcp_g_ 0.0625
Agent/TCP set windowInit_ 10; #initial window

Agent/TCP/FullTcp set segsize_ 1454
Agent/TCP/FullTcp set segsperack_ 1
Agent/TCP/FullTcp set spa_thresh_ 3000
Agent/TCP/FullTcp set interval_ 0
Agent/TCP/FullTcp set nodelay_ true
Agent/TCP/FullTcp set state_ 0
Agent/TCP/FullTcp set exp_id_ $expID

# if {[string compare $ccc_type "xpass"] == 0} {
    Agent/XPass set min_credit_size_ 84
    Agent/XPass set max_credit_size_ 84
    Agent/XPass set min_ethernet_size_ 84
    Agent/XPass set max_ethernet_size_ 1538
    Agent/XPass set max_credit_rate_ $creditRate
    Agent/XPass set base_credit_rate_ $baseCreditRate
    Agent/XPass set target_loss_scaling_ 0.125
    Agent/XPass set alpha 1.0
    Agent/XPass set w_init 0.0625
    Agent/XPass set min_w_ 0.01
    Agent/XPass set retransmit_timeout_ 0.0001
    Agent/XPass set min_jitter_ -0.1
    Agent/XPass set max_jitter_ 0.1
    Agent/XPass set exp_id_ $expID
# } elseif {([string compare $ccc_type "gdx"] == 0) || ([string compare $ccc_type "mqgdx"] == 0)} {
    Agent/TCP/FullTcp/XPass set w_init_ $w_init
    Agent/TCP/FullTcp/XPass set exp_id_ $expID
    Agent/TCP/FullTcp/XPass set target_loss_scaling_ 0.125
    Agent/TCP/FullTcp/XPass set base_credit_rate_ $baseCreditRate
    Agent/TCP/FullTcp/XPass set max_credit_rate_ $creditRate
    Agent/TCP/FullTcp/XPass set alpha_ $ALPHA
    Agent/TCP/FullTcp/XPass set min_credit_size_ 78
    Agent/TCP/FullTcp/XPass set max_credit_size_ 90
    Agent/TCP/FullTcp/XPass set min_ethernet_size_ 78
    Agent/TCP/FullTcp/XPass set max_ethernet_size_ 1538
    Agent/TCP/FullTcp/XPass set w_init_ 0.0625
    Agent/TCP/FullTcp/XPass set min_w_ 0.01
    Agent/TCP/FullTcp/XPass set retransmit_timeout_ 0.001
    Agent/TCP/FullTcp/XPass set default_credit_stop_timeout_ 0.001
    Agent/TCP/FullTcp/XPass set min_jitter_ -0.1
    Agent/TCP/FullTcp/XPass set max_jitter_ 0.1
    Agent/TCP/FullTcp/XPass set xpass_hdr_size_ 78
    # if {[string compare $ccc_type "mqgdx"] == 0} {
        
    # }
# } elseif {([string compare $ccc_type "egdx"] == 0) || ([string compare $ccc_type "negdx"] == 0)} {
    Agent/TCP/FullTcp/Egdx set min_credit_size_ 84
    Agent/TCP/FullTcp/Egdx set max_credit_size_ 84
    Agent/TCP/FullTcp/Egdx set min_ethernet_size_ 84
    Agent/TCP/FullTcp/Egdx set max_ethernet_size_ 1538
    Agent/TCP/FullTcp/Egdx set max_credit_rate_ $creditRate
    Agent/TCP/FullTcp/Egdx set base_credit_rate_ $baseCreditRate
    Agent/TCP/FullTcp/Egdx set target_loss_scaling_ 0.125
    Agent/TCP/FullTcp/Egdx set alpha 1.0
    Agent/TCP/FullTcp/Egdx set w_init 0.0625
    Agent/TCP/FullTcp/Egdx set min_w_ 0.01
    Agent/TCP/FullTcp/Egdx set retransmit_timeout_ 0.0001
    Agent/TCP/FullTcp/Egdx set min_jitter_ -0.1
    Agent/TCP/FullTcp/Egdx set max_jitter_ 0.1
    Agent/TCP/FullTcp/Egdx set exp_id_ $expID
    Agent/TCP/FullTcp/Egdx set egdx_beta_ $actual_xpass_queue_weight
    Agent/TCP/FullTcp/Egdx set rc3_mode_ 1
    Agent/TCP/FullTcp/Egdx set windowInit_ 5; #initial window
    Agent/TCP/FullTcp/Egdx set new_allocation_logic_ $newEgdxAllocationLogic;
    Agent/TCP/FullTcp/Egdx set reordering_measure_in_rc3_ $reordering_measure_in_rc3_;
    Agent/TCP/FullTcp/Egdx set static_allocation_ $static_allocation_;
# }

DelayLink set avoidReordering_ true
#{[expr $i * 2] <= $N} {
for {set idx 0} {$idx < [expr $N * $flowPerHost]} {incr idx} {
    set i [expr $idx / $flowPerHost];
  if {!([string compare $sender_type "legacy"] == 0) && (([string compare $sender_type "new"] == 0) || ([expr $idx * 2] < [expr $N * $flowPerHost]))} {
    if {[string compare $ccc_type "xpass"] == 0} {
        puts "XPass Agent"
        set sender($idx) [new Agent/XPass]
        set receiver($idx) [new Agent/XPass]
    } elseif {[string compare $ccc_type "gdx"] == 0} {
        puts "GDX Agent"
        set sender($idx) [new Agent/TCP/FullTcp/XPass]
        set receiver($idx) [new Agent/TCP/FullTcp/XPass]
    } elseif {[string compare $ccc_type "mqgdx"] == 0} {
        puts "MQ-GDX Agent"
        set sender($idx) [new Agent/TCP/FullTcp/XPass]
        set receiver($idx) [new Agent/TCP/FullTcp/XPass]
        # $sender($idx) set dctcp_ false;
        # $receiver($idx) set dctcp_ false;
        # $sender($idx) set dctcp_alpha_ 1.0;
        # $receiver($idx) set dctcp_alpha_ 1.0;
    } elseif {[string compare $ccc_type "egdx"] == 0} {
        puts "E-GDX Agent"
        set sender($idx) [new Agent/TCP/FullTcp/Egdx]
        set receiver($idx) [new Agent/TCP/FullTcp/Egdx]
    } elseif {[string compare $ccc_type "negdx"] == 0} {
        puts "E-GDX Agent (New)"
        set sender($idx) [new Agent/TCP/FullTcp/Egdx]
        set receiver($idx) [new Agent/TCP/FullTcp/Egdx]
    }
    $receiver($idx) attach $vt
    $receiver($idx) trace cur_credit_rate_tr_
    $receiver($idx) listen
  } else {
    puts "TCP Agent"
    set sender($idx) [new Agent/TCP/FullTcp/Sack]
    set receiver($idx) [new Agent/TCP/FullTcp/Sack]
    $sender($idx) attach $vt
    $sender($idx) trace cwnd_
    $receiver($idx) listen
  }

  $ns attach-agent $left_node($i) $sender($idx)
  $ns attach-agent $right_node($i) $receiver($idx)

  $sender($idx) set fid_ [expr $idx]
  $receiver($idx) set fid_ [expr $idx]

  $ns connect $sender($idx) $receiver($idx)
}

puts "Simulation started."
set nextTime 0.001
for {set i 0} {$i < [expr $N * $flowPerHost]} {incr i} {
  
  $ns at $nextTime "$sender($i) advance-bytes 1000000"
  set nextTime [expr $nextTime + $interFlowDelay]
}

# $ns at 9.9 "$bottleneck_queue_lr print-stat-queue"
$ns at 9.9 "$left_bcm print-egress-stat"
# $ns at 9.9 "$receiver(0) print-stat"
$ns at 10 "finish"
$ns run