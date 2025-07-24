{{< layout}}
  {{$body}}
    >todos

    `<title`new todo>  `[add`{{url:rns_todos}}`{{method:post}}|title]
    {{#error:title}}{{error_message:title}}{{/error:title}}

    -

    {{^todos_data}}
      no todos
    {{/todos_data}}
    {{#todos_data}}
      {{> todo_item}}
    {{/todos_data}}
  {{/body}}
{{/layout}}
