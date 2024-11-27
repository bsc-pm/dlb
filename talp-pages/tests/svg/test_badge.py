from talp_pages.svg.badge import Badge, BadgeResult


def test_badge_svg():
    result = BadgeResult(parallel_eff=0.6, ressource_configuration="1MPI")
    badge = Badge(result)
    assert badge.file_name.endswith(".svg")
    content = badge.get_content()
    assert content
