gprof ./bindex> gprof_output.txt
gprof2dot gprof_output.txt > call_graph.dot
dot -Tpng call_graph.dot -o call_graph.png
