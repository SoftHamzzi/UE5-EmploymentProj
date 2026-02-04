---
name: subagent-creator
description: Creates new subagent configuration files in .claude/agents/
disable-model-invocation: true
argument-hint: [agent-name]
---

Create a new subagent by generating a Markdown file in `.claude/agents/`.

## Process

### 1. Gather Requirements

If the user provided `$ARGUMENTS`, use it as the agent name. Otherwise ask:

- **Name**: lowercase, hyphens only (e.g., `code-reviewer`, `test-runner`)
- **Purpose**: What task does this agent handle?
- **Language**: What language should the agent respond in? (default: Korean with English technical terms)

### 2. Determine Configuration

Based on the purpose, decide:

| Field | How to decide |
|-------|---------------|
| `model` | `haiku` for fast search/lookup, `sonnet` for general tasks, `opus` for complex reasoning |
| `tools` | Only grant what's needed. Read-only tasks → `Read, Glob, Grep, WebSearch, WebFetch`. Code changes → add `Edit, Write, Bash` |

### 3. Write the Agent File

Create `.claude/agents/{name}.md` with this structure:

```markdown
---
name: {name}
description: "{1-2 sentence description of when Claude should delegate to this agent. Include example triggers.}"
model: {model}
---

{System prompt: role definition, methodology, output format, guidelines}
```

### 4. Description Field Guidelines

The `description` field is critical — Claude uses it to decide when to auto-delegate. Write it to include:

- **When** to use this agent (trigger conditions)
- **Example user messages** that should activate it (use `<example>` blocks)
- Format examples in the pattern used by existing agents in `.claude/agents/`

### 5. System Prompt Guidelines

The body after frontmatter is the agent's system prompt. Include:

1. **Role definition**: Who is this agent? What expertise does it have?
2. **Core mission**: One clear sentence on what it does
3. **Methodology**: Step-by-step process the agent should follow
4. **Output format**: How to structure the response
5. **Operational guidelines**: Rules and constraints
6. **Language**: Match the project convention (Korean with English technical terms)

### 6. Verify

After creating the file, read it back and confirm with the user.

## Reference

Check existing agents in `.claude/agents/` for style consistency before writing.
