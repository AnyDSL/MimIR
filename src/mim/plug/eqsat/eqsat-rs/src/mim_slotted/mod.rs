use crate::ffi::bridge::{CostFn, RewriteResult, RuleSet};
use crate::ffi::rec_expr_to_res_slotted;
use crate::mim_slotted::analysis::MimSlottedAnalysis;
use crate::mim_slotted::rulesets::get_rules;
use slotted_egraphs::*;

pub mod analysis;
pub mod rulesets;

define_language! {
    pub enum MimSlotted {
        // TERMS

        // (let <name> <definition> <expression>)
        Let(Bind<AppliedId>, AppliedId, AppliedId) = "let",
        // (lam <extern> <name> <var-name> <domain-type> <codomain-type> <filter> <body>)
        Lam(AppliedId, AppliedId, Bind<AppliedId>, AppliedId, AppliedId, AppliedId, AppliedId) = "lam",
        // (con <extern> <name> <var-name> <domain-type> <filter> <body>)
        Con(AppliedId, AppliedId, Bind<AppliedId>, AppliedId, AppliedId, AppliedId) = "con",
        // (app <callee> <arg>)
        App(AppliedId, AppliedId) = "app",
        // (var <name>)
        Var(Slot) = "var",
        // (lit <value> <type>)
        Lit(AppliedId, AppliedId) = "lit",
        // (pack <arity> <body>)
        Pack(AppliedId, AppliedId) = "pack",
        // (tuple <elem-cons>)
        Tuple(AppliedId) = "tuple",
        // (extract <tuple> <index>)
        Extract(AppliedId, AppliedId) = "extract",
        // (insert <tuple> <index> <value>)
        Insert(AppliedId, AppliedId, AppliedId) = "insert",
        // (rule <name> <meta_var> <lhs> <rhs> <guard>)
        Rule(AppliedId, AppliedId, AppliedId, AppliedId, AppliedId) = "rule",
        // (inj <type> <value>)
        Inj(AppliedId, AppliedId) = "inj",
        // (merge <type> <type-cons>)
        Merge(AppliedId, AppliedId) = "merge",
        // (axm <name> <type>)
        Axm(AppliedId, AppliedId) = "axm",
        // (match <op-cons>)
        Match(AppliedId) = "match",
        // (proxy <type> <pass> <tag> <op-cons>)
        Proxy(AppliedId, AppliedId, AppliedId, AppliedId) = "proxy",


        // TYPES

        // (join <type-cons>)
        Join(AppliedId) = "join",
        // (meet <type-cons>)
        Meet(AppliedId) = "meet",
        // (bot <type>)
        Bot(AppliedId) = "bot",
        // (top <type>)
        Top(AppliedId) = "top",
        // (arr <arity> <body>)
        Arr(AppliedId, AppliedId) = "arr",
        // (sigma <type-cons>)
        Sigma(AppliedId) = "sigma",
        // (cn <domain>)
        Cn(AppliedId) = "cn",
        // (pi <domain> <codomain>)
        Pi(AppliedId, AppliedId) = "pi",
        // (idx <size>)
        Idx(AppliedId) = "idx",
        // (hole <type>) - does it even make sense to have this?
        Hole(AppliedId) = "hole",
        // (type <level>)
        Type(AppliedId) = "type",
        // (reform <meta_type>)
        Reform(AppliedId) = "reform",


        // Enables variadic language constructs such as Tuple, Sigma, Match, ...
        // (cons <elem> <next>)
        Cons(AppliedId, AppliedId) = "cons",
        Nil() = "nil",

        // Leaf nodes
        Num(i64),
        Symbol(Symbol),
    }
}

pub fn equality_saturate(
    sexpr: &str,
    rulesets: Vec<RuleSet>,
    cost_fn: CostFn,
) -> Vec<RewriteResult> {
    let normalized = sexpr.replace("\r\n", "\n");
    let mut sexprs: Vec<&str> = normalized.split("\n\n").collect();
    sexprs.retain(|s| !s.trim().is_empty());

    let rules = get_rules(rulesets);

    // TODO: Uncomment after implemented
    // convert_rules(&mut sexprs, &mut rules);

    match cost_fn {
        CostFn::AstSize => rewrite_sexprs(sexprs, rules, || AstSize),
        _ => panic!("Unknown cost function provided."),
    }
}

pub fn pretty(sexpr: &str, _line_len: usize) -> String {
    let normalized = sexpr.replace("\r\n", "\n");
    let mut sexprs: Vec<&str> = normalized.split("\n\n").collect();
    sexprs.retain(|s| !s.trim().is_empty());
    let mut res = String::new();

    for (i, sexpr) in sexprs.iter().enumerate() {
        let parsed: RecExpr<MimSlotted> = RecExpr::parse(sexpr).unwrap();
        res.push_str(&parsed.to_string());
        if i < sexprs.len() - 1 {
            res.push_str("\n\n");
        } else {
            res.push('\n');
        }
    }

    res
}

fn rewrite_sexprs<C, F>(
    sexprs: Vec<&str>,
    rules: Vec<Rewrite<MimSlotted, MimSlottedAnalysis>>,
    cost_fn: F,
) -> Vec<RewriteResult>
where
    C: CostFunction<MimSlotted>,
    F: Fn() -> C,
{
    let mut rewritten_sexprs: Vec<RewriteResult> = Vec::new();

    for sexpr in sexprs {
        let mut runner = Runner::<MimSlotted, MimSlottedAnalysis, ()>::default()
            .with_expr(&RecExpr::parse(sexpr).unwrap());

        let _report = runner.run(&rules);

        let extractor = Extractor::new(&runner.egraph, cost_fn());
        let best_expr = extractor.extract(&runner.roots[0], &runner.egraph);

        rewritten_sexprs.push(rec_expr_to_res_slotted(best_expr));
    }

    rewritten_sexprs
}

// TODO: Implement
/*
fn convert_rules(sexprs: &mut Vec<&str>, rules: &mut Vec<Rewrite<MimSlotted, MimSlottedAnalysis>>) {
    // Converts rewrite rules in sexpr form into rewrite rules usable in egg and then
    // filters them out so we only have proper sexprs remaining to equality saturate in the next loop
    sexprs.retain(|sexpr| {
        let parsed: RecExpr<MimSlotted> = sexpr.parse().unwrap();
        if let Some((_id, MimSlotted::Rule(name, meta_var, lhs, rhs, _guard))) =
            parsed.items().last()
        {
            let nth_node = |id: AppliedId| parsed.items().nth(usize::from(id)).unwrap().1.clone();

            let rule_name = if let Mim::Symbol(s) = nth_node(*name) {
                s
            } else {
                panic!("Failed to parse rule name.")
            };

            let mut meta_vars: Vec<String> = Vec::new();
            let meta_var_rexpr = nth_node(*meta_var).build_recexpr(nth_node);
            for (_id, node) in meta_var_rexpr.items() {
                if let MimSlotted::Var(ids) = node
                    && let [var_name, ..] = &**ids
                {
                    if let Some((_id, MimSlotted::Symbol(s))) =
                        meta_var_rexpr.items().nth(usize::from(*var_name))
                    {
                        meta_vars.push(s.to_string().clone());
                    } else {
                        panic!("Failed to parse meta variable name.")
                    };
                }
            }

            let mut lhs_rexpr = nth_node(lhs).build_recexpr(nth_node);
            for (_id, node) in lhs_rexpr.items_mut() {
                if let MimSlotted::Symbol(s) = node
                    && meta_vars.contains(s)
                {
                    s.insert(0, '?')
                }
            }

            let mut rhs_rexpr = nth_node(*rhs).build_recexpr(nth_node);
            for (_id, node) in rhs_rexpr.items_mut() {
                if let Mim::Symbol(s) = node
                    && meta_vars.contains(s)
                {
                    s.insert(0, '?')
                }
            }

            let pat: Pattern<MimSlotted> = lhs_rexpr.pretty(80).parse().unwrap();
            let outpat: Pattern<MimSlotted> = rhs_rexpr.pretty(80).parse().unwrap();
            let rule: Rewrite<MimSlotted, MimSlottedAnalysis> =
                Rewrite::new(rule_name, pat, outpat).unwrap();
            rules.push(rule);
            false
        } else {
            true
        }
    });
}*/
