import re
import sys
import os

def get_instruction_to_opcode() -> dict: # converts and returns the serial outs of 'hippo8_microcode.ino' into a dictionary
    with open("opcodes_from_arduino.txt") as f:
        lines          = f.read().splitlines()
        lines_no_enter = [line for line in lines if len(line) > 0] # removes empty strings

    instruction_to_opcode = {}
    for line in lines_no_enter:
        s1, s2      = line.split('=>')
        instruction = s1.split('\t')[0]                     # extracts the instruction name, removes '\t'
        opcode      = re.search(r'0x[0-9a-f]+', s2).group() # extracts the hex opcode (eg: '0xFF', '0x12')
        
        num_params = 0 # 0, 1, 2
        if '{val}' in instruction:
            num_params = 1
        elif '{addr}' in instruction:
            num_params = 2

        instruction_to_opcode[instruction] = {
            "opcode":     int(opcode, 0),
            "operand_type": ["none", "imm8", "imm16"][num_params]
        }

    return instruction_to_opcode

OPCODES = get_instruction_to_opcode()

def process_included_file(lines: list[str], base_path=".") -> list[str]: # recursively adds included external files upon encountering '.include' keyword
    lines_with_included = []
    for line in lines:
        line = line.strip()
        if line.startswith(".include"): # appends the contents of whatever external files we include
            include_match = re.match(r'\.include\s+"([^"]+)"', line)
            if include_match:
                included_filename = os.path.join(base_path, include_match.group(1))
                if os.path.exists(included_filename):
                    with open(included_filename) as f:
                        included_lines = f.readlines()
                    lines_with_included.extend(process_included_file(included_lines, base_path))
                else:
                    raise FileNotFoundError(f"included file not found!: {included_filename}")
        else: # simply append the line
            lines_with_included.append(line)

    return lines_with_included

def strip_comments_and_whitespace(lines: list[str]) -> list[str]: # removes comments, trailing whitespace, and empty lines
    return [line.split(';')[0].strip() for line in lines if line.strip() and not line.strip().startswith(';')]

def extract_macros(lines: list[str]) -> tuple: # catalogs defined macros and removes their definitions from the source lines
    macros              = {}
    lines_no_macro_defs = []

    i = 0
    while i < len(lines):
        line = lines[i]

        if line.startswith(".macro"): # check if we are dealing with a macro definition
            # we grab the macro name and arguments it requires
            tokens                   = line.split()
            macro_name, macro_params = tokens[1], tokens[2:]
            i += 1

            # we travers through the macro and record its contents
            macro_contents = []
            while not lines[i].startswith(".endm"):
                macro_contents.append(lines[i])
                i += 1
            macros[macro_name] = macro_params, macro_contents
        else:
            lines_no_macro_defs.append(line)
        i += 1

    return lines_no_macro_defs, macros

MAXIMUM_MACRO_RECURSION_DEPTH = 100 # safety feature to limit to how many macros we can nest
def expand_macros(lines: list[str], macros: dict, call_stack=[]) -> list[str]:
    if len(call_stack) > MAXIMUM_MACRO_RECURSION_DEPTH:
        raise RecursionError(f"exceeded maximum macro expansion depth of {MAXIMUM_MACRO_RECURSION_DEPTH}: {' -> '.join(call_stack)}!")
    
    lines_with_expanded_macros = []
    for line in lines:
        tokens = line.split()
        if tokens[0] in macros: # check if a current line is a macro
            macro_name = tokens[0]
            if macro_name in call_stack:
                raise RuntimeError(f"circular macro definition detected: {' -> '.join(call_stack + [macro_name])}")

            macro_args                  = tokens[1:]
            macro_params, macro_content = macros[macro_name]
            expanded_macro              = []
            for macro_line in macro_content: # replace macro declaration with its full definition with correct argument assignment
                expanded_line = macro_line
                for param, arg in zip(macro_params, macro_args):
                    expanded_line = re.sub(rf"\b{param}\b", arg, expanded_line)
                expanded_macro.append(expanded_line)
            
            # recursively expand nested macros (macros that use other macros)
            lines_with_expanded_macros.extend(expand_macros(expanded_macro, macros, call_stack + [macro_name]))
        else:
            lines_with_expanded_macros.append(line)
    
    return lines_with_expanded_macros

def get_instruction_size(line: str) -> int: # returns the number of bytes the current instruction occupies in memory
    num_bytes = 1                                    # paramaterless instructions are only 1 byte long
    if '#' in line:                                  # detects imm8 parameter
        num_bytes = 2                                # instruction (1 byte) + imm8 (1 byte) = 2 bytes
    elif re.search(r"\b0x[0-9a-fA-F]{1,4}\b", line): # detects addr16 parameter
        num_bytes = 3                                # instruction (1 byte) + addr16 (2 bytes) =  3 bytes
    return num_bytes
    
def extract_labels(lines: list) -> tuple: # catalogs and removes label definitions
    labels              = {} # catalogs label definitions
    address             = 0  # tracks our position in memory
    lines_no_label_defs = []
    
    for line in lines:
        label_match = re.match(r"(\w+):", line)
        if label_match:
            label_name         = label_match.group(1)
            labels[label_name] = address
        else:
            lines_no_label_defs.append(line)
            address += get_instruction_size(line)

    return lines_no_label_defs, labels

def resolve_labels(line: str, labels: dict) -> str: # replaces existing labels in a single line with its corresponding address
    for label in labels:
        if label in line:
            label_address = labels[label]
            line = line.replace(label, f"0x{label_address:04x}") # 4 digit hex address
    return line

def assemble_instruction(line: list) -> list: # assembles a single instruction into bytecode
    tokens           = line.replace(',', ' ').split()
    instruction_name = tokens[0]
    instruction_args = tokens[1:]

    num_args = len(tokens) - 1
    match num_args:
        case 0:
            instruction_key = instruction_name
        case 1:
            arg     = instruction_args[0]
            operand = ''

            # R
            # address
            # [RCD]
            if arg.startswith('R'):
                operand = arg
            elif arg.startswith('0x'):
                operand = "{addr}"
            elif arg == "[RCD]":
                operand = "[RCD]"
            else:
                raise ValueError(f"unknown operand: '{arg}' in '{line}'")
            instruction_key = f"{instruction_name} {operand}"
        case 2:
            arg1, arg2 = instruction_args[0], tokens[1]
            operands   = ''

            # R, R
            # R, value
            # R, address
            # address, R
            # R, [RCD]
            # [RCD], R
            # SP, RCD
            # RCD, SP
            # value, R
            if arg1.startswith('R') and arg2.startswith('R'):
                operands = f"{arg1},{arg2}"
            elif arg1.startswith('R') and arg2.startswith('#'):
                operands = f"{arg1},#{{val}}"
            elif arg1.startswith('R') and arg2.startswith('0x'):
                operands = f"{arg1},{{addr}}"
            elif arg1.startswith('0x') and arg2.startswith('R'):
                operands = f"{{addr}},{arg2}"
            elif arg1.startswith('R') and arg2 == '[RCD]':
                operands = f"{arg1},[RCD]"
            elif arg1 == '[RCD]' and arg2.startswith('R'):
                operands = f"[RCD],{arg2}"
            elif arg1 == 'SP' and arg2 == 'RCD':
                operands = "SP,RCD"
            elif arg1 == 'RCD' and arg2 == 'SP':
                operands = "RCD,SP"
            elif arg1.startswith('#') and arg2.startswith('R'):
                operands = f"#{{val}},{arg2}"
            else:
                raise ValueError(f"unknown operands: '{arg1}' and/or '{arg2}' in '{line}'")

            instruction_key = f"{instruction_name} {operands}"
        case 3:
            arg1, arg2, arg3 = instruction_args[0], instruction_args[1], instruction_args[2]
            operands = ''

            # R, address, R
            # R, [address], R
            # address, R, R
            # [address], R, R
            if arg1.startswith('R') and arg2.startswith('0x'):
                operands = f"{arg1},{{addr}},{arg3}"
            elif arg1.startswith('R') and arg2.startswith('['):
                operands = f"{arg1},[{{addr}}],{arg3}"
            elif arg1.startswith('0x'):
                operands = f"{{addr}},{arg2},{arg3}"
            elif arg1.startswith('['):
                operands = f"[{{addr}}],{arg2},{arg3}"
            else:
                raise ValueError(f"unknown operands: '{arg1}' and/or '{arg2}' and/or '{arg3}' in '{line}'")
            
            instruction_key = f"{instruction_name} {operands}"
        case _:
            raise ValueError(f"unknown instruction format: {line}")
    
    instruction = OPCODES.get(instruction_key)
    if not instruction:
        raise ValueError(f"unknown instruction: {line}")

    opcode, operand_type = instruction["opcode"], instruction["operand_type"]
    match operand_type:
        case "none":
            return [opcode]
        case "imm8":
            for arg in instruction_args:
                if arg.startswith('#'):
                    imm8 = int(arg.replace('#', ''))
                    if imm8 > 0xFF:
                        raise ValueError(f"#{{val}} exceeds 8-bits: {line}")
                    else:
                        break
            return [opcode, imm8]
        case "addr16":
            for arg in instruction_args:
                if arg.startswith('0x'):
                    addr16 = int(arg, 0)
                    if addr16 > 0xFFFF:
                        raise ValueError(f"{{addr}} exceeds 16-bits: {line}")
                    else:
                        break
            return [opcode, addr16 & 0xFF, (addr16 >> 8) & 0xFF]
        case _:
            raise ValueError(f"unknown instruction: {line}")

def assemble(lines: list[str]) -> list[int]:
    all_lines       = process_included_file(lines)             # recursively combine all of the included files into one big ass list containing the file lines
    sanitized_lines = strip_comments_and_whitespace(all_lines) # basic sanitization, remove extra spaces and comments
    
    # expand macros
    lines_no_macro_defs, macros = extract_macros(sanitized_lines)            # catalogs defined macros and removes their definitions from the source lines
    lines_expanded_macros       = expand_macros(lines_no_macro_defs, macros) # fully expand the macros


    # extract labels
    lines_no_label_defs, labels = extract_labels(lines_expanded_macros)

    # resolve labels and convert assembly instructions into bytecode
    bytecode = []
    for line in lines_no_label_defs:
        resolved_line = resolve_labels(line, labels)
        bytecode.extend(assemble_instruction(resolved_line))
    return bytecode

def main() -> None:
    if len(sys.argv) != 2:
        print("usage: python3 assembler.py <sourcefile.asm>")
        sys.exit(1)
    else:
        with open(sys.argv[1]) as f:
            lines = f.readlines()                # list 'lines' contains all of the lines from the source file
        bytecode = assemble(lines)
        with open("out.hex", "w") as out:
            out.write(' '.join(f'0x{byte:02X}' for byte in bytecode))
        print("completed! assembled to out.hex")

if __name__ == "__main__":
    main()
