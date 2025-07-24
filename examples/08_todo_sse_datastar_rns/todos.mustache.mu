{{< layout}}
  {{$body}}
    >todos

    `<title`new todo>  `[add`{{url:rns_todos}}`{{method:post}}|title]

    -

    {{^todos_data}}
      no todos
    {{/todos_data}}
    {{#todos_data}}
      {{> todo_item}}
    {{/todos_data}}
  {{/body}}
{{/layout}}
