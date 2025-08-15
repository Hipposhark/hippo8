"""
custom assembler for the hippo8

help from Slu4 for giving ideas for the procedure
- https://www.youtube.com/watch?v=rdKX9hzA2lU
"""


import sys
import os
import re

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

OPCODES           = get_instruction_to_opcode()
RESERVED_KEYWORDS = set(opcode.split()[0] for opcode in OPCODES) | {'RA', 'RB', 'RC', 'RD', 'SP', 'RCD'} # can't use these keywords as user-defined names (eg. variables and label names)

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

def evaluate_expression_with_variables(expression: str, variables: dict) -> str:
    expression      = expression.replace('#', '')

    local_namespace = {k: evaluate_expression_with_variables(v, variables) for k, v in variables.items() if k in expression}
    try:
        return str(eval(expression, {"__builtins__": {}}, local_namespace))
    except Exception as e:
        raise ValueError(f"Invalid expression '{expression}': {e}")

def extract_variables(lines: list[str]) -> tuple: # catalogs variable declarations and removes them from the source lines
    variables                      = {}
    lines_no_variable_declarations = []

    for line in lines:
        variable_match = re.match(r"^\s*(\w+)\s*=\s*(.+)$", line) # checks for '=' syntax
        if variable_match:
            name  = variable_match.group(1)
            value = variable_match.group(2).strip()

            # check if the variable name we used is a reserved keyword
            if name.upper() in RESERVED_KEYWORDS:
                raise ValueError(f"variable name '{name}' conflicts with a reserved keyword.")
            elif name in variables:
                raise ValueError(f"variable name '{name}' is already in use somewhere!")
            else:
                evaluated_value = evaluate_expression_with_variables(value, variables)
                variables[name] = evaluated_value
        else:
            lines_no_variable_declarations.append(line)
    return lines_no_variable_declarations, variables
    
def substitute_variables(lines: list[str], variables: dict) -> list[str]: # returns the lines with variables substituted  
    lines_substitued = []
    for line in lines:
        for name, value in variables.items():
            line = re.sub(rf'\b{name}\b', value, line) # replaces the variable with its value
        lines_substitued.append(line)
    return lines_substitued

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
            
            # check if macro name conflicts with any reserved keywords
            if macro_name.upper() in RESERVED_KEYWORDS:
                raise ValueError(f"macro name '{macro_name}' conflicts with a reserved keyword.")
            else:
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
    elif re.search(r"\b0x[0-9a-fA-F]{1,4}\b", line): # detects imm16 parameter
        num_bytes = 3                                # instruction (1 byte) + imm16 (2 bytes) =  3 bytes
    
    curr_instruction_mneomonic = line.split()[0]
    if curr_instruction_mneomonic[0] == 'J': # detects a jump instruction
        num_bytes = 3

    return num_bytes

BASE_ADDRESS = 0xE000 # the address where program data starts with respect to entire memory
def extract_labels(lines: list) -> tuple: # catalogs and removes label definitions
    labels              = {}            # catalogs label definitions
    address             = BASE_ADDRESS  # tracks our position in memory
    lines_no_label_defs = []
    
    for line in lines:
        label_match = re.match(r"(\.?[A-Za-z_]+):", line) # searches for ':'
        if label_match:
            label_name         = label_match.group(1)

            # check if label name conflicts with a reserved keyword
            if label_name.upper() in RESERVED_KEYWORDS:
                raise ValueError(f"label name '{label_name}' conflicts with a reserved keyword.")
            else:
                labels[label_name] = address
        else:
            lines_no_label_defs.append(line)
            address += get_instruction_size(line)

    return lines_no_label_defs, labels

def resolve_labels(line: str, labels: dict) -> str: # replaces existing labels in a single line with its corresponding address
    for label in labels:
        if label in line:
            label_address = labels[label]

            label_pattern = rf"\b{re.escape(label)}\b" # replace exact label literal
            line          = re.sub(label_pattern, f"0x{label_address:04x}", line) # 4 digit hex address
    return line

def evaluate_expression_with_no_variables(expression: str) -> str:
    expression = expression.replace('#', '')
    try:
        return str(eval(expression, {"__builtins__": {}}))
    except Exception as e:
        raise ValueError(f"Invalid expression '{expression}': {e}")

def get_resolved_operand_to_integer(arg: str) -> int: # returns the unsigned integer form if the argument is an immediate value, returns -1 if not
    # eg. #5 -> 5, 

    integer = -1
    if arg.startswith('#'):
        arg     = evaluate_expression_with_no_variables(arg)
        integer = int(arg.replace('#', ''), 0)
    elif arg.startswith('0x'):
        arg     = evaluate_expression_with_no_variables(arg)
        integer = int(arg, 0)
    elif arg.startswith('['): # accounts for direct indexing and 
        integer = int(arg.replace('[', '').replace(']', ''), 0)
    elif arg in ['RA', 'RB', 'RC', 'RD', 'SP', '[RCD]']:
        pass
    else:
        raise ValueError(f"Unknown argument format: {arg}")
        
    return integer

def assemble_instruction(line: list) -> list: # assembles a single instruction into bytecode
    tokens           = line.replace(',', ' ').split()
    instruction_name = tokens[0]
    instruction_args = tokens[1:]

    # we parse the instruction and identify its operand types
    num_args = len(tokens) - 1
    match num_args:
        case 0:
            instruction_key = instruction_name
        case 1:
            arg     = instruction_args[0]
            operand = ''

            # 1 operand combos:
            # R
            # address
            # [RCD]
            if arg.startswith('R'):
                operand = arg
            elif arg.startswith('['):
                if arg == "[RCD]":
                    operand = "[RCD]"
                else:
                    operand = "{addr}"
            elif arg.startswith('0x'):
                operand = "{addr}"
            else:
                raise ValueError(f"unknown operand: '{arg}' in '{line}'")
            instruction_key = f"{instruction_name} {operand}"
        case 2:
            arg1, arg2 = instruction_args[0], instruction_args[1]
            operands   = ''

            # 2 operand combos
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
            elif arg1.startswith('R') and arg2.startswith('['):
                if arg2 == '[RCD]':
                    operands = f"{arg1},[RCD]"
                else:
                    operands = f"{arg1},{{addr}}"
            elif arg1.startswith('[') and arg2.startswith('R'):
                operands = f"{{addr}},{arg2}"
            elif arg1 == '[RCD]' and arg2.startswith('R'):
                operands = f"[RCD],{arg2}"
            elif arg1 == 'SP' and arg2 == 'RCD':
                operands = "SP,RCD"
            elif arg1 == 'RCD' and arg2 == 'SP':
                operands = "RCD,SP"
            elif arg1.startswith('#') and arg2.startswith('R'):
                operands = f"{{val}},{arg2}"
            else:
                raise ValueError(f"unknown operands: '{arg1}' and/or '{arg2}' in '{line}'")

            instruction_key = f"{instruction_name} {operands}"
        case 3:
            arg1, arg2, arg3 = instruction_args[0], instruction_args[1], instruction_args[2]
            operands = ''

            # 3 operand combos:
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
    
    # we index the instruction into our catalog of instructions to retrieve its related information
    instruction = OPCODES.get(instruction_key)
    if not instruction:
        raise ValueError(f"unknown instruction: {instruction_key}")

    # we convert the assembly tokenized instruction into its corresponding bytearray representation
    opcode, operand_type = instruction["opcode"], instruction["operand_type"]
    bytecode             = []
    match operand_type:
        case "none":
            bytecode = [opcode]
        case "imm8":
            for arg in instruction_args:
                if get_resolved_operand_to_integer(arg) > -1:
                    imm8 = get_resolved_operand_to_integer(arg)
                    if imm8 > 0xFF:
                        raise ValueError(f"#{{val}} exceeds 8-bits: {line}")
                    else:
                        break
            bytecode = [opcode, imm8]
        case "imm16":
            for arg in instruction_args:
                if get_resolved_operand_to_integer(arg) > -1:
                    imm16 = get_resolved_operand_to_integer(arg)
                    if imm16 > 0xFFFF:
                        raise ValueError(f"{{addr}} exceeds 16-bits: {line}")
                    else:
                        break
            bytecode = [opcode, imm16 & 0xFF, (imm16 >> 8) & 0xFF]
        case _:
            raise ValueError(f"unknown instruction: {line}")
    
    # print(f"{tokens} \t-> {bytecode}")

    return bytecode

def assemble(lines: list[str]) -> list[int]:
    all_lines       = process_included_file(lines)             # recursively combine all of the included files into one big ass list containing the file lines
    sanitized_lines = strip_comments_and_whitespace(all_lines) # basic sanitization, remove extra spaces and comments

    # substitute variables
    lines_no_vars, variables = extract_variables(sanitized_lines)
    lines_subbed_vars        = substitute_variables(lines_no_vars, variables)

    # expand macros
    lines_no_macro_defs, macros = extract_macros(lines_subbed_vars)            # catalogs defined macros and removes their definitions from the source lines
    lines_expanded_macros       = expand_macros(lines_no_macro_defs, macros) # fully expand the macros

    # extract labels
    lines_no_label_defs, labels = extract_labels(lines_expanded_macros)
    print(labels)

    # resolve labels and convert assembly instructions into bytecode
    bytecode = []
    for line in lines_no_label_defs:
        resolved_line = resolve_labels(line, labels)
        # print(resolved_line)
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

        # we format the output so its easy to program in Arduino
        bytecode_formatted = []

        curr_line = ''
        for address, byte in enumerate(bytecode):
            if address % 16 == 0:                          # header every 16 bytes with the address
                curr_line = f"/* 0x{address:04x} */"
            
            curr_line     += f" 0x{byte:02x},"
            is_end_of_line = (address % 16 == 15)
            is_last_byte   = (address == len(bytecode) - 1)

            if is_last_byte:                               # if this is the last line, we remove the trailing comma
                bytecode_formatted.append(curr_line[:-1])  # add the byte line to output
                break

            if is_end_of_line:
                curr_line += f'\n'                         # adds a return when we get to the end of 16 bytes      
                bytecode_formatted.append(curr_line)       # add the byte line to output

        # we write the data to out.hex to be pasted into and programmed in Arduino!
        file_name = sys.argv[1].rstrip('.asm')
        with open(f"{file_name}.hex", "w") as out:
            for line in bytecode_formatted:
                out.write(line)
        print(f"completed! assembled to '{file_name}.hex'")
        for line in bytecode_formatted:
            print(line)

if __name__ == "__main__":
    main()
