use std::str::FromStr;

use anyhow::bail;

#[derive(Debug, Clone)]
enum Annotation {
    Get,
    Set,
    GetSet,
    Type {
        path: Option<String>,
        name: String,
    },
    Func {
        path: String,
        name: String,
    },
    Method {
        path: String,
        name: String,
    },
    Metamethod {
        path: String,
        metamethod: Metamethod,
    },
}

impl FromStr for Annotation {
    type Err = anyhow::Error;

    fn from_str(mut s: &str) -> Result<Self, Self::Err> {
        if s.starts_with("//@") {
            s = s.trim_start_matches("//@");
        }

        let parts = s.split_whitespace().collect::<Vec<_>>();
        let annotation = match (parts.get(0), parts.get(1)) {
            (None, None) => bail!("Empty annotation"),

            (Some(&"get"), None) => Self::Get,
            (Some(&"set"), None) => Self::Set,
            (Some(&"getset"), None) => Self::GetSet,

            (Some(&"type"), None) => bail!("@type requires argument `name`"),
            (Some(&"type"), Some(name)) => match name.rsplit_once(".") {
                None => Self::Type {
                    path: None,
                    name: name.to_string(),
                },
                Some((path, name)) => Self::Type {
                    path: Some(path.to_string()),
                    name: name.to_string(),
                },
            },

            (Some(&"func"), None) => bail!("@func requires argument `name`"),
            (Some(&"func"), Some(name)) => match name.rsplit_once(".") {
                None => bail!("@func's `name` must contain `.`"),
                Some((path, name)) => Self::Func {
                    path: path.to_string(),
                    name: name.to_string(),
                },
            },

            (Some(&"method"), None) => bail!("@method requires argument `name`"),
            (Some(&"method"), Some(name)) => match name.rsplit_once(":") {
                None => bail!("@method's `name` must contain `:`"),
                Some((path, name)) => Self::Method {
                    path: path.to_string(),
                    name: name.to_string(),
                },
            },

            (Some(&"metamethod"), None) => bail!("@metamethod requires argument `name`"),
            (Some(&"metamethod"), Some(name)) => {
                let Some((path, name)) = name.rsplit_once(":") else {
                    bail!("@metamethod's `name` must contain `:`");
                };

                let metamethod = match name {
                    "__index" => Metamethod::Index,
                    "__newindex" => Metamethod::Newindex,
                    "__add" => Metamethod::Add,
                    _ => bail!("@metamethod's `name` is unknown (\"{name}\")"),
                };

                Self::Metamethod {
                    path: path.to_string(),
                    metamethod,
                }
            }

            _ => bail!("Unknown annotation"),
        };

        Ok(annotation)
    }
}

#[derive(Debug, Clone)]
enum Metamethod {
    Index,
    Newindex,
    Add,
}
