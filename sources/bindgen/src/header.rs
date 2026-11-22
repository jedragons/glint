use lazy_static::lazy_static;
use tree_sitter::{Query, QueryCursor, QueryMatch, StreamingIterator};

lazy_static! {
    static ref TYPE: Query = Query::new(
        &tree_sitter_c::LANGUAGE.into(),
        r"(
            (comment)+ @comment
            (type_definition
                type: (struct_specifier
                    name: (type_identifier) @name
                    body: (field_declaration_list
                        (
                            (comment)* @field_comment
                            (field_declaration
                                type: (primitive_type) @field_type
                                declarator: (field_identifier) @field_name))* @field)))
        )"
    )
    .expect("Invalid query");
    static ref TYPE_COMMENT_INDEX: u32 = TYPE
        .capture_index_for_name("comment")
        .expect("comment should be captured in type");
    static ref TYPE_NAME_INDEX: u32 = TYPE
        .capture_index_for_name("name")
        .expect("name should be captured in type");
    static ref TYPE_FIELD_COMMENT_INDEX: u32 = TYPE
        .capture_index_for_name("field_comment")
        .expect("field_comment should be captured in type");
    static ref TYPE_FIELD_TYPE_INDEX: u32 = TYPE
        .capture_index_for_name("field_type")
        .expect("field_type should be captured in type");
    static ref TYPE_FIELD_NAME_INDEX: u32 = TYPE
        .capture_index_for_name("field_name")
        .expect("field_name should be captured in type");
    static ref FUNCTION: Query = Query::new(
        &tree_sitter_c::LANGUAGE.into(),
        r"(
            (comment)+ @comment
            .
            (declaration
                type: (_) @return_type
                declarator: (function_declarator
                    declarator: (identifier) @name
                    parameters: (parameter_list
                        (comment)* @param_comment
                        (parameter_declaration
                            type: (type_identifier) @param_type
                            declarator: (_) @param_name)) @params))
        )"
    )
    .expect("Invalid query");
    static ref FUNCTION_COMMENT_INDEX: u32 = FUNCTION
        .capture_index_for_name("comment")
        .expect("comment should be captured in function");
    static ref FUNCTION_RETURN_TYPE_INDEX: u32 = FUNCTION
        .capture_index_for_name("return_type")
        .expect("comment should be captured in function");
    static ref FUNCTION_NAME_INDEX: u32 = FUNCTION
        .capture_index_for_name("name")
        .expect("comment should be captured in function");
    static ref FUNCTION_PARAMS_INDEX: u32 = FUNCTION
        .capture_index_for_name("params")
        .expect("params should be captured in function");
}

#[derive(Debug, Clone, Default)]
pub struct Header {
    name: String,
    types: Vec<Type>,
    functions: Vec<Function>,
}

impl Header {
    pub fn parse(name: &str, source: &str) -> anyhow::Result<Self> {
        let mut parser = tree_sitter::Parser::new();
        parser
            .set_language(&tree_sitter_c::LANGUAGE.into())
            .expect("Could not set language");

        let tree = parser.parse(&source, None).expect("Could not parse");
        let mut header = Self::new(name.to_string());

        let mut qc = QueryCursor::new();
        let mut matches = qc.matches(&TYPE, tree.root_node(), source.as_bytes());
        while let Some(m) = matches.next() {
            match Type::parse_from_match(m, source) {
                Ok(t) => header.types.push(t),
                Err(e) => eprintln!("Could not parse type: {e:#}"),
            }
        }

        let mut qc = QueryCursor::new();
        let mut matches = qc.matches(&FUNCTION, tree.root_node(), source.as_bytes());
        while let Some(m) = matches.next() {
            match Function::parse_from_match(m, &source) {
                Ok(f) => header.functions.push(f),
                Err(e) => eprintln!("Could not parse function: {e:#}"),
            }
        }

        Ok(header)
    }

    fn new(name: String) -> Self {
        Self {
            name,
            ..Default::default()
        }
    }
}

#[derive(Debug, Clone, Default)]
struct Type {
    name: String,
    comments: Vec<String>,
    fields: Vec<Field>,
}

#[derive(Debug, Clone, Default)]
struct Field {
    comments: Vec<String>,
    type_: String,
    name: String,
}

impl Type {
    fn parse_from_match(m: &QueryMatch<'_, '_>, source: &str) -> anyhow::Result<Type> {
        let comments = m
            .nodes_for_capture_index(*TYPE_COMMENT_INDEX)
            .map(|n| source[n.byte_range()].to_string())
            .collect::<Vec<_>>();

        let name = m.nodes_for_capture_index(*TYPE_NAME_INDEX).next().unwrap();
        let name = source[name.byte_range()].to_string();

        let mut fields = Vec::new();
        let mut current_field = Field::default();
        for c in m.captures() {
            let text = &source[c.node.byte_range()];
            match TYPE.capture_names()[c.index as usize] {
                "field_comment" => current_field.comments.push(text.to_string()),
                "field_type" => current_field.type_ = text.to_string(),
                "field_name" => {
                    current_field.name = text.to_string();
                    fields.push(std::mem::take(&mut current_field));
                }
                _ => {}
            }
        }

        Ok(Self {
            name,
            comments,
            fields,
        })
    }
}

#[derive(Debug, Clone, Default)]
struct Function {
    name: String,
    comments: Vec<String>,
    return_type: String,
    parameters: Vec<Parameter>,
}

#[derive(Debug, Clone, Default)]
struct Parameter {
    name: String,
    comments: Vec<String>,
    type_: String,
}

impl Function {
    fn parse_from_match(m: &QueryMatch<'_, '_>, source: &str) -> anyhow::Result<Function> {
        let comments = m
            .nodes_for_capture_index(*FUNCTION_COMMENT_INDEX)
            .map(|n| source[n.byte_range()].to_string())
            .collect::<Vec<_>>();

        let name = m
            .nodes_for_capture_index(*FUNCTION_NAME_INDEX)
            .next()
            .unwrap();
        let name = source[name.byte_range()].to_string();

        let return_type = m
            .nodes_for_capture_index(*FUNCTION_RETURN_TYPE_INDEX)
            .next()
            .unwrap();
        let return_type = source[return_type.byte_range()].to_string();

        let mut parameters = Vec::new();
        let mut current_param = Parameter::default();
        for c in m.captures() {
            let text = &source[c.node.byte_range()];
            match FUNCTION.capture_names()[c.index as usize] {
                "param_comment" => current_param.comments.push(text.to_string()),
                "param_type" => current_param.type_ = text.to_string(),
                "param_name" => {
                    current_param.name = text.to_string();
                    parameters.push(std::mem::take(&mut current_param));
                }
                _ => {}
            }
        }

        Ok(Self {
            name,
            comments,
            return_type,
            parameters,
        })
    }
}
