PROFILER_SO := profiler.so
MEM_PROFILER := mem_profiler.so
HTML_CONTENT := html_content.h
HTML_HEADER_VAR := htmlHeader
HTML_FOOTER_VAR := htmlFooter
COMPILER_OPT := -g -std=c++0x -rdynamic

clean:
	rm -rf $(PROFILER_SO) $(HTML_CONTENT)

$(HTML_CONTENT): header.html footer.html
	rm -rf $(HTML_CONTENT)
	./conv_html_to_c header.html $(HTML_CONTENT) $(HTML_HEADER_VAR)
	./conv_html_to_c footer.html $(HTML_CONTENT) $(HTML_FOOTER_VAR)

htmlcontent: $(HTML_CONTENT)

$(PROFILER_SO): $(HTML_CONTENT) profiler.cpp profiler.h common.cpp common.h
	g++ -o $(PROFILER_SO) $(COMPILER_OPT) -shared -fpic profiler.cpp common.cpp -lpthread

$(MEM_PROFILER): $(HTML_CONTENT) mem_profiler.cpp mem_profiler.h common.cpp common.h
	g++ -o $(MEM_PROFILER) $(COMPILER_OPT) -shared -fpic mem_profiler.cpp common.cpp -ldl -DMALLOC_FUNC_NAME=malloc -DFREE_FUNC_NAME=free
	
all: $(PROFILER_SO) $(MEM_PROFILER)

test:
	profiler strace.test strace.test.html

