all:
	@make --no-print-directory -C sandbox

clean:
	@make --no-print-directory clean -C sandbox
run:all
	@./sandbox/vmx /root/workspace/tmp/a.out

