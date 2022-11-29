set follow-fork-mode child
break init
break spawn_child
break inner_child
run
set follow-fork-mode parent
continue
set follow-fork-mode child
continue
