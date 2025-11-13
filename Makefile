# Compilador e flags
CXX := g++
CXXFLAGS := -Wall -Wextra -pthread -std=c++17

# Pastas
SRC_DIR := src
BUILD_DIR := build

# Arquivos
TARGET := $(BUILD_DIR)/peer
SRC := $(SRC_DIR)/main.cpp $(SRC_DIR)/Peer.cpp $(SRC_DIR)/FileProcessor.cpp $(SRC_DIR)/Protocol.cpp
OBJ := $(patsubst $(SRC_DIR)/%.cpp, $(BUILD_DIR)/%.o, $(SRC))

CONFIG_FILE := tests/test1.conf
FILE_TO_SHARE := data/exemplo.txt
BLOCK_SIZE := 1024

all: $(TARGET)

# Compila o executável final
$(TARGET): $(OBJ)
	@echo "🔧 Linking..."
	$(CXX) $(CXXFLAGS) -o $@ $^

# Compila o objeto
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(BUILD_DIR)
	@echo "⚙️  Compilando $<..."
	$(CXX) $(CXXFLAGS) -c $< -o $@

# ---------------------------------
# Gera o metadata do arquivo base
# ---------------------------------
meta:
	@echo "📦 Gerando metadados de '$(FILE_TO_SHARE)'..."
	@$(TARGET) --create-meta $(FILE_TO_SHARE) $(BLOCK_SIZE)

# ---------------------------------
# Lê o arquivo teste especifico e inicia os peers
# ---------------------------------
run:
	@if [ -z "$(TEST)" ]; then \
		echo "❌ Uso: make run TEST=data/tests/test1_2peers_small_1KB.config"; exit 1; \
	fi
	@if [ ! -f $(TEST) ]; then \
		echo "❌ Arquivo $(TEST) não encontrado!"; exit 1; \
	fi
	@echo "🚀 Executando configuração $(TEST)..."

	# Inicia Seeders
	@grep "^SEEDER" $(TEST) | while read -r _ port meta neighbors; do \
		echo "🌱 Seeder $$port -> $$neighbors"; \
		gnome-terminal -- bash -c "$(TARGET) --meta $$meta $$port $$neighbors; exec bash" & \
		sleep 2; \
	done

	# Inicia Leechers
	@grep "^LEECHER" $(TEST) | while read -r _ port neighbors; do \
		echo "📥 Leecher $$port -> $$neighbors"; \
		gnome-terminal -- bash -c "$(TARGET) $$port $$neighbors; exec bash" & \
		sleep 0.5; \
	done

	@echo "✅ Todos os peers foram inicializados!"

# -------------------------
#  Limpeza
# -------------------------

clean:
	@echo "🧹 Limpando arquivos compilados..."
	rm -rf $(BUILD_DIR)

# Evita conflito com arquivos chamados "clean" ou "all"
.PHONY: all clean run
