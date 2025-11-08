# Compilador e flags
CXX := g++
CXXFLAGS := -Wall -Wextra -pthread -std=c++17

# Pastas
SRC_DIR := src
BUILD_DIR := build

# Arquivos
TARGET := $(BUILD_DIR)/peer
SRC := $(SRC_DIR)/main.cpp $(SRC_DIR)/Peer.cpp $(SRC_DIR)/FileProcessor.cpp
OBJ := $(patsubst $(SRC_DIR)/%.cpp, $(BUILD_DIR)/%.o, $(SRC))


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

# -------------------------
#  Execução simplificada
# -------------------------

# Executa dois peers locais (exemplo)
run:
	@echo "🚀 Iniciando Peer A (porta 5000, vizinho 127.0.0.1:5001)..."
	@gnome-terminal -- bash -c "$(TARGET) 5000 127.0.0.1 5001; exec bash" &
	@sleep 1
	@echo "🚀 Iniciando Peer B (porta 5001, vizinho 127.0.0.1:5000)..."
	@gnome-terminal -- bash -c "$(TARGET) 5001 127.0.0.1 5000; exec bash" &

# -------------------------
#  Limpeza
# -------------------------

clean:
	@echo "🧹 Limpando arquivos compilados..."
	rm -rf $(BUILD_DIR)

# Evita conflito com arquivos chamados "clean" ou "all"
.PHONY: all clean run
