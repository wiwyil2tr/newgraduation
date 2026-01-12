# 根目录 Makefile

.PHONY: all backend frontend modules clean install test

all: backend modules frontend

backend:
	@echo "编译后端..."
	$(MAKE) -C backend

frontend:
	@echo "编译前端..."
	$(MAKE) -C frontendqt

modules:
	@echo "编译模块..."
	$(MAKE) -C modules/scanner

clean:
	@echo "清理..."
	$(MAKE) -C backend clean
	$(MAKE) -C frontend clean
	$(MAKE) -C modules/scanner clean

install:
	@echo "安装..."
	$(MAKE) -C backend install
	$(MAKE) -C frontend install
	$(MAKE) -C modules/scanner install
