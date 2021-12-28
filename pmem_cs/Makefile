CFLAGS = -std=c++11 -lpthread -O3 -Wall -Wextra -Werror -mavx2 -lpmem -lpmemobj

ct.out: column-sketches.cpp
	$(CXX) $^ $(CFLAGS) -o $@

test: ct.out uniform zipf operator tpch

uniform:
	./run_uniform.sh

zipf:
	./run_zipf.sh

operator:
	./run_operator.sh

report:
	./report.sh

clean:
	rm -rf *.out
	rm -rf logs
	rm -rf report
	rm -rf run_tpch.sh

clean-log:
	rm -rf logs

clean-report:
	rm -rf report

.PHONY: uniform operator tpch report clean clean-log clean-report
